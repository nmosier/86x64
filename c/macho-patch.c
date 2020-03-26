#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xed/xed-interface.h>

#include "macho.h"
#include "macho-patch.h"
#include "macho-util.h"
#include "util.h"

int macho_patch(union macho *macho) {
   switch (macho_kind(macho)) {
   case MACHO_FAT:
      fprintf(stderr, "macho_patch: patching fat binary not supported\n");
      return -1;

   case MACHO_ARCHIVE:
      return macho_patch_archive(&macho->archive);
   }
}

int macho_patch_archive(union archive *archive) {
   switch (macho_bits(archive)) {
   case MACHO_32:
      return macho_patch_archive_32(&archive->archive_32);
      
   case MACHO_64:
      fprintf(stderr, "macho_patch_archive: patching 64-bit archive not supported\n");
      return -1;
   }
}

int macho_patch_archive_32(struct archive_32 *archive) {
   uint32_t ncmds = archive->header.ncmds;

   struct segment_32 *seg_text, *seg_data;
   seg_text = macho_find_segment_32(SEG_TEXT, archive);
   seg_data = macho_find_segment_32(SEG_DATA, archive);
   if (seg_text) {
      if (macho_patch_TEXT(seg_text) < 0) { return -1; }
   }
   if (seg_data) {
      if (macho_patch_DATA(seg_data, seg_text) < 0) { return -1; }
   }

   for (uint32_t i = 0; i < ncmds; ++i) {
      union load_command_32 *command = &archive->commands[i];
      struct dyld_info *dyld;

      switch (command->load.cmd) {
      case LC_DYLD_INFO:
      case LC_DYLD_INFO_ONLY:
         dyld = &command->dyld_info;
         if (macho_patch_dyld_info(dyld, archive) < 0) { return -1; }
         break;
         
      default:
         break;
      }
   }
   
   return 0;
}

int macho_patch_TEXT(struct segment_32 *text) {
   uint32_t nsects = text->command.nsects;

   /* reconstruct old addresses */
   macho_off_t *old_addrs;
   if ((old_addrs = calloc(nsects, sizeof(old_addrs[0]))) == NULL) {
      perror("calloc");
      return -1;
   }
   for (uint32_t secti = 0; secti < nsects; ++secti) {
      struct section_wrapper_32 *sectwr = &text->sections[secti];
      old_addrs[secti] = sectwr->section.addr - sectwr->adiff;
   }
   qsort(old_addrs, nsects, sizeof(old_addrs[0]),
         (int (*)(const void *, const void *)) macho_off_cmp);
   
   /* patch each section */
   for (uint32_t secti = 0; secti < nsects; ++secti) {
      struct section_wrapper_32 *sectwr = &text->sections[secti];
      struct section *section = &sectwr->section;
      uint8_t *start = sectwr->data;
      uint8_t *end = start + section->size;

      /* check if section should be patched */
      if (!(section->flags & S_ATTR_PURE_INSTRUCTIONS)) {
         continue;
      }

      for (uint8_t *it = start; it < end; ) {
         xed_decoded_inst_t xedd;
         xed_state_t dstate;
         xed_error_enum_t err;

         xed_tables_init();
         xed_state_zero(&dstate);
         dstate.mmode = XED_MACHINE_MODE_LEGACY_32;
         dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b;
         xed_decoded_inst_zero_set_mode(&xedd, &dstate);
         xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
         
         /* decode instruction */
         if ((err = xed_decode(&xedd, it, end - it)) != XED_ERROR_NONE) {
            fprintf(stderr, "macho_patch_TEXT: xed_decode (section %*s, addr 0x%x): %s\n",
                    (int) sizeof(section->sectname), section->sectname,
                    (uint32_t) (it - start + section->addr - sectwr->adiff),
                    xed_error_enum_t2str(err));
            return -1;
         }

         /* OPERANDS REQUIRING PATCHING
          * - Memory, direct
          * - Relative jump outside of section
          * - Relative call outside of section
          * - Absolute jump
          * - Absolute call
          */

         unsigned int noperands = xed_decoded_inst_noperands(&xedd);
         xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);

         /* Memory accesses */
         unsigned int memops = xed_operand_values_number_of_memory_operands(operands);
         for (unsigned int i = 0; i < noperands; ++i) {
            xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(operands, i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == XED_REG_ESI && indexreg == XED_REG_INVALID &&
                xed_operand_values_has_memory_displacement(operands)) {
               /* get old address of this instruction */
               macho_addr_t old_inst_addr = (it - start) + section->addr - sectwr->adiff;

               /* get memory displacement */
               xed_int64_t old_memdisp = xed_decoded_inst_get_memory_displacement(operands, i);

               /* compute old target address */
               macho_addr_t old_target_addr = old_inst_addr + old_memdisp;
               
               /* translate to new address */
               macho_addr_t new_target_addr = macho_patch_TEXT_address(old_target_addr, text);

               /* compute new memory displacement */
               xed_int64_t new_memdisp = old_memdisp + new_target_addr - old_target_addr;

               /* patch instruction with new memory displacement */
               xed_enc_displacement_t disp =
                  {.displacement = new_memdisp,
                   .displacement_bits = xed_decoded_inst_get_memory_displacement_width(operands, i)
                  };
               if (!xed_patch_disp(&xedd, it, disp)) {
                  fprintf(stderr, "xed_path_disp: failed to patch instruction at address 0x%llx\n",
                          old_inst_addr);
                  return -1;
               }
            }
         }

         /* update byte iterator */
         it += xedd._decoded_length;
      }
   }

   return 0;
}

macho_addr_t macho_patch_TEXT_address(macho_addr_t addr, const struct segment_32 *segment) {
   const uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct section_wrapper_32 *sectwr = &segment->sections[i];
      const struct section *section = &sectwr->section;
      macho_addr_t adjusted_addr = addr + sectwr->adiff;
      if (adjusted_addr >= section->addr &&
          adjusted_addr < section->addr + section->size) {
         return adjusted_addr;
      }
   }

   return MACHO_BAD_ADDR;
}

int macho_patch_DATA(struct segment_32 *data_seg, const struct segment_32 *text_seg) {
   uint32_t nsects = data_seg->command.nsects;
   
   /* iterate thru sections */
   for (uint32_t i = 0; i < nsects; ++i) {
      struct section_wrapper_32 *sectwr = &data_seg->sections[i];
      struct section *section = &sectwr->section;

      if ((section->flags & (S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS))) {
         /* patch addresses */
         uint32_t *addrs = sectwr->data;
         size_t naddrs = section->size / sizeof(addrs[0]);

         // DEBUG
         switch ((section->flags & (S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS))) {
         case S_NON_LAZY_SYMBOL_POINTERS:
            printf("S_NON_LAZY_SYMBOL_POINTERS [0x%x]:", section->addr);
            for (size_t i = 0; i < naddrs; ++i) {
               printf(" 0x%x", addrs[i]);
            }
            printf("\n");
            break;
         }
         
         
         for (size_t i = 0; i < naddrs; ++i) {
            macho_addr_t new_addr;
            if (addrs[i] == 0) {
               continue;
            }
            
            if ((new_addr = macho_patch_symbol_pointer(addrs[i], text_seg)) == MACHO_BAD_ADDR) {
               fprintf(stderr, "macho_patch_data: macho_patch_symbol_pointer: 0x%x: bad address\n",
                       addrs[i]);
               return -1;
            }
            addrs[i] = new_addr;
         }
      }
   }

   return 0;
}

macho_addr_t macho_patch_symbol_pointer(macho_addr_t addr, const struct segment_32 *text) {
   const uint32_t nsects = text->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct section_wrapper_32 *sectwr = &text->sections[i];
      struct section *section = &sectwr->section;

      macho_addr_t new_addr = addr + sectwr->adiff;
      if (new_addr >= section->addr && new_addr < section->addr + section->size) {
         return new_addr;
      }
   }

   return MACHO_BAD_ADDR;
}

int macho_patch_dyld_info(struct dyld_info *dyld, struct archive_32 *archive) {
   /* patch rebase info */
   uint8_t *rebase_begin = dyld->rebase_data;
   uint8_t *rebase_end = rebase_begin + dyld->command.rebase_size;
   for (uint8_t *rebase_it = rebase_begin;
        rebase_it != dyld->rebase_data + dyld->command.rebase_size; ) {
      
      /* NOTE: Just look for REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB opcodes. */
      
      uint8_t opcode = *rebase_it & REBASE_OPCODE_MASK;
      uint8_t imm = *rebase_it & REBASE_IMMEDIATE_MASK;
      uintmax_t uleb;
      size_t uleb_len;
      size_t rebase_rem;
      bool uleb_adjust = false;
      int32_t seg_index = -1;
      
      ++rebase_it;
      
      switch (opcode) {
      case REBASE_OPCODE_DONE:
         return 0;
         
      case REBASE_OPCODE_SET_TYPE_IMM:
      case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
      case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
         break;
         
      case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         uleb_adjust = true;
         seg_index = imm; // TODO
         /* fallthrough */
      case REBASE_OPCODE_ADD_ADDR_ULEB:
      case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
      case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
      case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
         rebase_rem = rebase_end - rebase_it;
         uleb_len = uleb128_decode(rebase_it, rebase_rem, &uleb);
         if (uleb_len == 0) {
            fprintf(stderr, "macho_patch_dyld_info: unsigned LEB128 value is too long\n");
            return -1;
         } else if (uleb_len > rebase_rem) {
            fprintf(stderr,
                    "macho_patch_dyld_info: unsigned LEB128 value runs past end of rebase data\n");
            return -1;
         }

         if (uleb_adjust) {
            struct segment_32 *segment;
            macho_addr_t new_addr;
            uintmax_t new_uleb;

            /* compute new ULEB offset */
            if ((segment = macho_index_segment_32(seg_index, archive)) == NULL) {
               fprintf(stderr, "macho_patch_dyld_info: segment %d not found\n", seg_index);
               return -1;
            }
            if ((new_addr = macho_patch_symbol_pointer
                 (uleb + segment->command.vmaddr - segment->adiff, segment))
                == MACHO_BAD_ADDR) {
               fprintf(stderr, "macho_patch_dyld_info: bad address 0x%jx\n", uleb);
               return -1;
            }
            new_uleb = new_addr - segment->command.vmaddr;
            
            /* check if different number of bits */
            size_t new_uleb_len = uleb128_encode(NULL, SIZE_MAX, new_uleb);
            if (new_uleb_len != uleb_len) {
               fprintf(stderr, "macho_patch_dyld_info: changing length of unsigned LEB128 not "\
                       "supported yet (0x%jx -> 0x%jx)\n", uleb, new_uleb);
               return -1;
            }

            uleb128_encode(rebase_it, new_uleb_len, new_uleb);
         }

         rebase_it += uleb_len;
         break;
         
      default:
         fprintf(stderr, "macho_patch_dyld_info: invalid rebase opcode 0x%x\n", opcode);
         return -1;
      }
   }

   return 0;
}
