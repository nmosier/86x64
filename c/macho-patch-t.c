#if MACHO_BITS == 32

# define SYMTAB symtab_32
# define DYSYMTAB dysymtab_32
# define SEGMENT segment_32
# define SECTION_WRAPPER section_wrapper_32
# define ARCHIVE archive_32
# define LOAD_COMMAND load_command_32
# define SECTION section
# define NLIST nlist

# define macho_patch_TEXT_address   macho_patch_TEXT_address_32
# define macho_patch_archive        macho_patch_archive_32
# define macho_patch_TEXT           macho_patch_TEXT_32
# define macho_patch_DATA           macho_patch_DATA_32
# define macho_patch_dyld_info      macho_patch_dyld_info_32
# define macho_patch_symbol_pointer macho_patch_symbol_pointer_32
# define macho_patch_address        macho_patch_address_32
# define macho_patch_dyld_info_rebase_dst macho_patch_dyld_info_rebase_dst_32
# define macho_patch_dyld_info_do_rebase macho_patch_dyld_info_do_rebase_32
# define macho_patch_rebase_info    macho_patch_rebase_info_32
# define macho_patch_bind_info      macho_patch_bind_info_32
# define macho_patch_uleb           macho_patch_uleb_32

# define macho_find_segment         macho_find_segment_32
# define macho_index_segment        macho_index_segment_32
# define macho_vmaddr_to_section    macho_vmaddr_to_section_32

# define MACHO_SIZE_T uint32_t
# define MACHO_ADDR_T uint32_t

#else

# define SYMTAB symtab_64
# define DYSYMTAB dysymtab_64
# define SEGMENT segment_64
# define SECTION_WRAPPER section_wrapper_64
# define ARCHIVE archive_64
# define LOAD_COMMAND load_command_64
# define SECTION section_64
# define NLIST nlist_64

# define macho_patch_TEXT_address   macho_patch_TEXT_address_64
# define macho_patch_archive        macho_patch_archive_64
# define macho_patch_TEXT           macho_patch_TEXT_64
# define macho_patch_DATA           macho_patch_DATA_64
# define macho_patch_dyld_info      macho_patch_dyld_info_64
# define macho_patch_symbol_pointer macho_patch_symbol_pointer_64
# define macho_patch_address        macho_patch_address_64
# define macho_patch_dyld_info_rebase_dst macho_patch_dyld_info_rebase_dst_64
# define macho_patch_dyld_info_do_rebase macho_patch_dyld_info_do_rebase_64
# define macho_patch_rebase_info    macho_patch_rebase_info_64
# define macho_patch_bind_info      macho_patch_bind_info_64
# define macho_patch_uleb           macho_patch_uleb_64

# define macho_find_segment         macho_find_segment_64
# define macho_index_segment        macho_index_segment_64
# define macho_vmaddr_to_section    macho_vmaddr_to_section_64

# define MACHO_SIZE_T uint64_t
# define MACHO_ADDR_T uint64_t

#endif

#include <assert.h>
#include <stdlib.h>

#include "macho.h"
#include "macho-util.h"

int macho_patch_TEXT(struct SEGMENT *text, const struct ARCHIVE *archive);
int macho_patch_DATA(struct SEGMENT *data_seg, const struct SEGMENT *text_seg);
MACHO_ADDR_T macho_patch_symbol_pointer(MACHO_ADDR_T addr, const struct SEGMENT *text);
struct SEGMENT *macho_index_segment(uint32_t index, struct ARCHIVE *archive);
struct SECTION_WRAPPER *macho_vmaddr_to_section(MACHO_ADDR_T addr, struct SEGMENT *segment);
static int macho_patch_dyld_info_rebase_dst(MACHO_ADDR_T addr, struct SEGMENT *segment,
                                            struct ARCHIVE *archive);
static int macho_patch_dyld_info_do_rebase(MACHO_ADDR_T addr, uint8_t type,
                                           struct SEGMENT *segment, struct ARCHIVE *archive);
int macho_patch_dyld_info(struct dyld_info *dyld, struct ARCHIVE *archive);
static void *macho_patch_uleb(void *uleb_ptr, size_t size, struct SEGMENT *segment,
                              struct ARCHIVE *archive, MACHO_ADDR_T *new_addr);

MACHO_ADDR_T macho_patch_TEXT_address(MACHO_ADDR_T addr, const struct SEGMENT *segment) {
   const uint32_t nsects = segment->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &segment->sections[i];
      const struct SECTION *section = &sectwr->section;
      MACHO_ADDR_T adjusted_addr = addr + sectwr->adiff;
      if (adjusted_addr >= section->addr &&
          adjusted_addr < section->addr + section->size) {
         return adjusted_addr;
      }
   }

   return MACHO_BAD_ADDR;
}

MACHO_ADDR_T macho_patch_address(MACHO_ADDR_T addr, const struct ARCHIVE *archive) {
   const uint32_t ncmds = archive->header.ncmds;
   for (uint32_t cmdi = 0; cmdi < ncmds; ++cmdi) {
      union LOAD_COMMAND *command = &archive->commands[cmdi];
      if (command->load.cmd == (MACHO_BITS == 32 ? LC_SEGMENT : LC_SEGMENT_64)) {
         struct SEGMENT *segment = &command->segment;
         const uint32_t nsects = segment->command.nsects;
         for (uint32_t secti = 0; secti < nsects; ++secti) {
            struct SECTION_WRAPPER *sectwr = &segment->sections[secti];
            const struct SECTION *section = &sectwr->section;
            MACHO_ADDR_T adjusted_addr = addr + sectwr->adiff;
            if (adjusted_addr >= section->addr && adjusted_addr < section->addr + section->size) {
               return adjusted_addr;
            }
         }
      }
   }
   return MACHO_BAD_ADDR;
}

int macho_patch_archive(struct ARCHIVE *archive) {
   uint32_t ncmds = archive->header.ncmds;
   
   struct SEGMENT *seg_text, *seg_data;
   seg_text = macho_find_segment(SEG_TEXT, archive);
   seg_data = macho_find_segment(SEG_DATA, archive);
   if (seg_text) {
      if (macho_patch_TEXT(seg_text, archive) < 0) { return -1; }
   }
   if (seg_data) {
      if (macho_patch_DATA(seg_data, seg_text) < 0) { return -1; }
   }

   for (uint32_t i = 0; i < ncmds; ++i) {
      union LOAD_COMMAND *command = &archive->commands[i];
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


int macho_patch_TEXT(struct SEGMENT *text, const struct ARCHIVE *archive) {
   uint32_t nsects = text->command.nsects;

   /* reconstruct old addresses */
   MACHO_ADDR_T *old_addrs;
   if ((old_addrs = calloc(nsects, sizeof(old_addrs[0]))) == NULL) {
      perror("calloc");
      return -1;
   }
   for (uint32_t secti = 0; secti < nsects; ++secti) {
      struct SECTION_WRAPPER *sectwr = &text->sections[secti];
      old_addrs[secti] = sectwr->section.addr - sectwr->adiff;
   }
   qsort(old_addrs, nsects, sizeof(old_addrs[0]),
         (int (*)(const void *, const void *)) macho_off_cmp);
   
   /* patch each section */
   for (uint32_t secti = 0; secti < nsects; ++secti) {
      struct SECTION_WRAPPER *sectwr = &text->sections[secti];
      struct SECTION *section = &sectwr->section;
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

         // xed_tables_init();
         xed_state_zero(&dstate);
#if MACHO_BITS == 32
         dstate.mmode = XED_MACHINE_MODE_LEGACY_32;
#else
         dstate.mmode = XED_MACHINE_MODE_LONG_64;
#endif
         ;
         dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b;
         xed_decoded_inst_zero_set_mode(&xedd, &dstate);
         xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
         
         /* decode instruction */
         if ((err = xed_decode(&xedd, it, end - it)) != XED_ERROR_NONE) {
            // DEBUG
            printf("macho_patch_TEXT: xed_decode (section %*s, addr 0x%llx): %s\n",
                   (int) sizeof(section->sectname), section->sectname,
                   (uint64_t) (it - start + section->addr - sectwr->adiff),
                   xed_error_enum_t2str(err));
            ++it;
            continue;
         }


         // DEBUG
         if (xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_RET_FAR) {
            printf("macho_patch_TEXT: retf instruction\n");
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
         // unsigned int memops = xed_operand_values_number_of_memory_operands(operands);
         for (unsigned int i = 0; i < noperands; ++i) {
            xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(operands, i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == (MACHO_BITS == 32 ? XED_REG_EIP : XED_REG_RIP) &&
                indexreg == XED_REG_INVALID &&
                xed_operand_values_has_memory_displacement(operands)) {
               /* get old address of this instruction */
               macho_addr_t old_inst_addr = (it - start) + section->addr - sectwr->adiff;

               /* get memory displacement */
               xed_int64_t old_memdisp = xed_decoded_inst_get_memory_displacement(operands, i);

               /* compute old target address */ 
               MACHO_ADDR_T old_target_addr = old_inst_addr + xed_decoded_inst_get_length(&xedd) +
                  old_memdisp;
               
               /* translate to new address */
               MACHO_ADDR_T new_target_addr = macho_patch_address(old_target_addr, archive);

               /* compute new memory displacement */
               xed_int64_t new_memdisp = old_memdisp + new_target_addr - old_target_addr -
                  sectwr->adiff;

               /* patch instruction with new memory displacement */
               xed_enc_displacement_t disp =
                  {.displacement = new_memdisp,
                   .displacement_bits =
                   xed_decoded_inst_get_memory_displacement_width_bits(operands, i)
                  };
               if (!xed_patch_disp(&xedd, it, disp)) {
                  fprintf(stderr, "xed_path_disp: failed to patch instruction at address 0x%llx\n",
                          old_inst_addr);
                  return -1;
               }

               // DEBUG
               printf("[patched instruction at address 0x%llx/0x%llx]\n", old_inst_addr,
                      old_inst_addr + sectwr->adiff);
               
            }

            /* Jumps [RELATIVE] */
            if (xed_operand_values_has_branch_displacement(operands)) {
               /* old relative displacement of this branch */
               const xed_int32_t old_branchdisp = xed_decoded_inst_get_branch_displacement(operands);

               /* get $rip value that this displacement is relative to */
               const MACHO_ADDR_T inst_addr = (it - start) + section->addr - sectwr->adiff;
               const MACHO_ADDR_T base_addr = inst_addr + xed_decoded_inst_get_length(&xedd);

               /* compute old target address */
               const MACHO_ADDR_T old_target_addr = base_addr + old_branchdisp;

               /* translate to new address */
               const MACHO_ADDR_T new_target_addr = macho_patch_TEXT_address(old_target_addr, text);

               /* compute new displacement */
               const xed_int32_t new_branchdisp = old_branchdisp + new_target_addr -
                  old_target_addr - sectwr->adiff;

               /* patch instruction with new memory displacement */
               xed_encoder_operand_t disp =
                  {.type = XED_ENCODER_OPERAND_TYPE_BRDISP,
                   .u = {.brdisp = new_branchdisp},
                   .width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd)
                  };
               if (!xed_patch_relbr(&xedd, it, disp)) {
                  fprintf(stderr,
                          "xed_patch_relbr: failed to patch instruction at address 0x%llx\n",
                          (uint64_t) inst_addr);
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


int macho_patch_DATA(struct SEGMENT *data_seg, const struct SEGMENT *text_seg) {
   uint32_t nsects = data_seg->command.nsects;
   
   /* iterate thru sections */
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &data_seg->sections[i];
      struct SECTION *section = &sectwr->section;

      if ((section->flags & (S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS))
          && sectwr->data) {
         /* patch addresses */
         MACHO_ADDR_T *addrs = sectwr->data;
         MACHO_SIZE_T naddrs = section->size / sizeof(addrs[0]);

         // DEBUG
         const char *debug_fmt;
         switch ((section->flags & (S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS))) {
         case S_NON_LAZY_SYMBOL_POINTERS:
            debug_fmt = "S_NON_LAZY_SYMBOL_POINERS [0x%llx]:";
            break;
         case S_LAZY_SYMBOL_POINTERS:
            debug_fmt = "S_LAZY_SYMBOL_POINTERS [0x%llx]:";
            break;
         }
         printf(debug_fmt, (uint64_t) section->addr);
         for (MACHO_SIZE_T i = 0; i < naddrs; ++i) {
            printf(" 0x%llx", (uint64_t) addrs[i]);
         }
         printf("\n");
         
         for (MACHO_SIZE_T i = 0; i < naddrs; ++i) {
            MACHO_ADDR_T new_addr;
            if (addrs[i] == 0) {
               continue;
            }
            
            if ((new_addr = macho_patch_symbol_pointer(addrs[i], text_seg)) == MACHO_BAD_ADDR) {
               fprintf(stderr,
                       "macho_patch_data: macho_patch_symbol_pointer(0x%llx, %.*s, %.*s): " \
                       "0x%llx: bad address\n",
                       (uint64_t) (section->addr + sizeof(addrs[i]) * i - sectwr->adiff),
                       (int) sizeof(section->segname), section->segname,
                       (int) sizeof(section->sectname), section->sectname,
                       (uint64_t) addrs[i]);
               return -1;
            }
            addrs[i] = new_addr;
         }
      }
   }

   return 0;
}

static int macho_patch_rebase_info(struct dyld_info *dyld, struct ARCHIVE *archive);
static int macho_patch_bind_info(struct dyld_info *dyld, struct ARCHIVE *archive);

int macho_patch_dyld_info(struct dyld_info *dyld, struct ARCHIVE *archive) {
   if (macho_patch_rebase_info(dyld, archive) < 0) {
      return -1;
   }

   if (macho_patch_bind_info(dyld, archive) < 0) {
      return -1;
   }

   return 0;
}

static int macho_patch_rebase_info(struct dyld_info *dyld, struct ARCHIVE *archive) {
   /* patch rebase info */
   uint8_t *rebase_begin = dyld->rebase_data;
   const uint8_t *rebase_end = rebase_begin + dyld->command.rebase_size;

   int32_t seg_index = -1;
   uint8_t type = 0;
   struct SEGMENT *segment;
   MACHO_ADDR_T new_addr; /* rebase address */
   
   for (uint8_t *rebase_it = rebase_begin;
        rebase_it != rebase_end; ) {
      
      /* NOTE: Just look for REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB opcodes. */
      
      uint8_t opcode = *rebase_it & REBASE_OPCODE_MASK;
      uint8_t imm = *rebase_it & REBASE_IMMEDIATE_MASK;
      uintmax_t uleb;
      size_t uleb_len;
      // size_t rebase_rem;
      // bool uleb_adjust = false;
      
      ++rebase_it;

      size_t rebase_rem = rebase_end - rebase_it;
      
      switch (opcode) {
      case REBASE_OPCODE_DONE:
         return 0;

      case REBASE_OPCODE_SET_TYPE_IMM:
         type = imm;
         break;
         
      case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
         for (uint8_t i = 0; i < imm; ++i) {
            if (macho_patch_dyld_info_do_rebase(new_addr, type, segment, archive) < 0)
               { return -1; }
            new_addr += sizeof(MACHO_ADDR_T);
         }
         break;

      case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         seg_index = imm;
         if ((segment = macho_index_segment(seg_index, archive)) == NULL) {
            fprintf(stderr, "macho_patch_dyld_info: segment %d not found\n", seg_index);
            return -1;
         }
         if ((rebase_it =
              macho_patch_uleb(rebase_it, rebase_rem, segment, archive, &new_addr)) == NULL)
            { return -1; }
         break;
         

      case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
         /* do rebase */
         if (macho_patch_dyld_info_do_rebase(new_addr, type, segment, archive) < 0) { return -1; }
         new_addr += sizeof(MACHO_ADDR_T);
         /* fallthrough */
      case REBASE_OPCODE_ADD_ADDR_ULEB:
         uleb_len = uleb128_decode(rebase_it, rebase_rem, &uleb);
         if (uleb_len == 0) {
            fprintf(stderr, "macho_patch_dyld_info: unsigned LEB128 overflow\n");
            return -1;
         } else if (uleb_len > rebase_rem) {
            fprintf(stderr,
                    "macho_patch_dyld_info: unsigned LEB128 runs past end of rebase info\n");
            return -1;
         }
         new_addr += uleb;
         rebase_it += uleb_len;
         break;

      case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
         /* read ULEB */
         uleb_len = uleb128_decode(rebase_it, rebase_rem, &uleb);
         if (uleb_len == 0) {
            fprintf(stderr, "macho_patch_dyld_info: unsigned LEB128 overflow\n");
            return -1;
         } else if (uleb_len > rebase_rem) {
            fprintf(stderr,
                    "macho_patch_dyld_info: unsigned LEB128 runs past end of rebase info\n");
            return -1;
         }
         rebase_it += uleb_len;

         /* do rebase */
         for (uintmax_t i = 0; i < uleb; ++i) {
            if (macho_patch_dyld_info_do_rebase(new_addr, type, segment, archive) < 0)
               { return -1; }
            new_addr += sizeof(MACHO_ADDR_T);
         }
         break;

      case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
         /* NOTE: Unclear what do scale by, but assume it's pointer size. */
         new_addr += imm * sizeof(MACHO_ADDR_T);
         break;

      case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
         {
            uintmax_t times, skip;

            /* read times ULEB */
            uleb_len = uleb128_decode(rebase_it, rebase_rem, &uleb);
            if (uleb_len == 0) {
               fprintf(stderr, "macho_patch_dyld_info: unsigned LEB128 overflow\n");
               return -1;
            } else if (uleb_len > rebase_rem) {
               fprintf(stderr,
                       "macho_patch_dyld_info: unsigned LEB128 runs past end of rebase info\n");
               return -1;
            }
            rebase_it += uleb_len;
            rebase_rem -= uleb_len;
            times = uleb;

            /* read skip ULEB */
            uleb_len = uleb128_decode(rebase_it, rebase_rem, &uleb);
            if (uleb_len == 0) {
               fprintf(stderr, "macho_patch_dyld_info: unsigned LEB128 overflow\n");
               return -1;
            } else if (uleb_len > rebase_rem) {
               fprintf(stderr,
                       "macho_patch_dyld_info: unsigned LEB128 runs past end of rebase info\n");
               return -1;
            }
            rebase_it += uleb_len;
            rebase_rem -= uleb_len;
            skip = uleb;

            /* do rebase */
            for (uintmax_t i = 0; i < times; ++i) {
               if (macho_patch_dyld_info_do_rebase(new_addr, type, segment, archive) < 0)
                  { return -1; }
               new_addr += sizeof(MACHO_ADDR_T) + skip;
            }
         }
         break;
         
      default:
         fprintf(stderr, "macho_patch_dyld_info: invalid rebase opcode 0x%x\n", opcode);
         return -1;
      }
   }
   return 0;   
}

static int macho_patch_bind_info(struct dyld_info *dyld, struct ARCHIVE *archive) {
   uint8_t *bind_begin = dyld->bind_data;
   const uint8_t *bind_end = bind_begin + dyld->command.bind_size;

   uint8_t type = BIND_TYPE_POINTER;
   struct SEGMENT *segment = NULL;
   MACHO_ADDR_T addr = 0;
         
   for (uint8_t *bind_it = bind_begin; bind_it != bind_end; ) {
      size_t uleb_len, sleb_len;
      uintmax_t uleb;
      
      const uint8_t opcode = (*bind_it & BIND_OPCODE_MASK);
      const uint8_t imm = (*bind_it & BIND_IMMEDIATE_MASK);

      ++bind_it;

      size_t bind_rem = bind_end - bind_it;
      
      switch (opcode) {
      case BIND_OPCODE_DONE:
         return 0; // TODO: This might not be correct.

      case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
      case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
         // TODO: How to manage reordering?
         break;

      case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
         uleb_len = uleb128_decode(bind_it, bind_rem, NULL);
         if (uleb_len > bind_rem) {
            fprintf(stderr, "macho_patch_bind_info: bind info ended inside ULEB\n");
            return -1;
         } else if (uleb_len == 0) {
            fprintf(stderr, "macho_patch_bind_info: ULEB overflow\n");
            return -1;
         }
         bind_it += uleb_len;
         break;

      case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
         /* symbol name follows */
         switch (imm) {
         case 0x0:
            while (*bind_it++ != '\0') {} /* skip symbol string */
            break;

         default:
            fprintf(stderr, "macho_patch_bind_info: unrecognized trailing flags type 0x%x\n", imm);
            return -1;
         }
         break;

      case BIND_OPCODE_SET_TYPE_IMM:
         type = imm;
         break;

      case BIND_OPCODE_SET_ADDEND_SLEB:
         sleb_len = sleb128_decode(bind_it, bind_rem, NULL);
         if (sleb_len == 0) {
            fprintf(stderr, "macho_patch_bind_info: SLEB overflow\n");
            return -1;
         } else if (sleb_len > bind_rem) {
            fprintf(stderr, "macho_patch_bind_info: bind info ends inside SLEB\n");
            return -1;
         }
         bind_it += sleb_len;
         break;

      case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         if ((segment = macho_index_segment(imm, archive)) == NULL) {
            fprintf(stderr, "macho_patch_bind_info: invalid segment index %d\n", imm);
            return -1;
         }
         if ((bind_it = macho_patch_uleb(bind_it, bind_rem, segment, archive, &addr)) == NULL) {
            return -1;
         }
         break;

      case BIND_OPCODE_ADD_ADDR_ULEB:
      case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
         uleb_len = uleb128_decode(bind_it, bind_rem, &uleb);
         if (uleb_len == 0) {
            fprintf(stderr, "macho_patch_bind_info: ULEB overflow\n");
            return -1;
         } else if (uleb_len > bind_rem) {
            fprintf(stderr, "macho_patch_bind_info: bind info ends inside ULEB\n");
            return -1;
         }
         addr += uleb;
         bind_it += uleb_len;
         break;

      case BIND_OPCODE_DO_BIND:
         break;

      case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
         addr += imm * sizeof(MACHO_ADDR_T);
         break;

      case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
         {
            uintmax_t ulebs[2];
            size_t uleb_lens[2];

            for (int i = 0; i < 2; ++i) {
               uleb_lens[i] = uleb128_decode(bind_it, bind_rem, &ulebs[i]);
               if (uleb_lens[i] == 0) {
                  fprintf(stderr, "macho_patch_bind_info: ULEB overflow\n");
                  return -1;
               } else if (uleb_lens[i] > bind_rem) {
                  fprintf(stderr, "macho_patch_bind_info: bind info ends inside ULEB\n");
                  return -1;
               }
               bind_it += uleb_lens[i];
               bind_rem -= uleb_lens[i];
            }

            addr += ulebs[0] * ulebs[1];
         }
         break;

      case BIND_OPCODE_THREADED:
         fprintf(stderr, "macho_patch_bind_info: BIND_OPCODE_THREADED unsupported\n");
         return -1;
         
      default:
         fprintf(stderr, "macho_patch_bind_info: unrecognized opcode 0x%02x\n", opcode);
         return -1;
      }
   }
   
   return 0;
}


MACHO_ADDR_T macho_patch_symbol_pointer(MACHO_ADDR_T addr, const struct SEGMENT *text) {
   const uint32_t nsects = text->command.nsects;
   for (uint32_t i = 0; i < nsects; ++i) {
      struct SECTION_WRAPPER *sectwr = &text->sections[i];
      struct SECTION *section = &sectwr->section;

      MACHO_ADDR_T new_addr = addr + sectwr->adiff;
      if (new_addr >= section->addr && new_addr < section->addr + section->size) {
         return new_addr;
      }
   }

   return MACHO_BAD_ADDR;
}

static void *macho_patch_uleb(void *uleb_ptr, size_t size, struct SEGMENT *segment,
                              struct ARCHIVE *archive, MACHO_ADDR_T *new_addr) {
   /* decode ULEB */
   uintmax_t uleb;
   size_t uleb_len = uleb128_decode(uleb_ptr, size, &uleb);

   if (uleb_len == 0) {
      fprintf(stderr, "macho_patch_dyld_info_rebase_uleb: unsigned LEB128 value is too long\n");
      return NULL;
   } else if (uleb_len > size) {
      fprintf(stderr,
              "macho_patch_dyld_info: unsigned LEB128 value runs past end of rebase data\n");
      return NULL;      
   }

   MACHO_ADDR_T new_addr_;
   uintmax_t new_uleb;

   /* compute new ULEB offset */
   if ((new_addr_ = macho_patch_symbol_pointer
        (uleb + segment->command.vmaddr - segment->adiff, segment))
       == MACHO_BAD_ADDR) {
      fprintf(stderr, "macho_patch_dyld_info: bad address 0x%jx\n", uleb);
      fprintf(stderr, "[segment %*s, base addr 0x%llx]\n",
              (int) sizeof(segment->command.segname), segment->command.segname,
              (uint64_t) (segment->command.vmaddr - segment->adiff));
      return NULL;
   }
   new_uleb = new_addr_ - segment->command.vmaddr;
            
   /* check if different number of bits */
   size_t new_uleb_len = uleb128_encode(NULL, SIZE_MAX, new_uleb);
   if (new_uleb_len != uleb_len) {
      fprintf(stderr, "macho_patch_dyld_info: changing length of unsigned LEB128 not "\
              "supported yet (0x%jx -> 0x%jx)\n", uleb, new_uleb);
      return NULL;
   }

   uleb128_encode(uleb_ptr, new_uleb_len, new_uleb);
   
   if (new_addr) { *new_addr = new_addr_; }

   return (char *) uleb_ptr + new_uleb_len;
}

static int macho_patch_dyld_info_rebase_dst(MACHO_ADDR_T addr, struct SEGMENT *segment,
                                            struct ARCHIVE *archive) {
   struct SECTION_WRAPPER *sectwr;
   if ((sectwr = macho_vmaddr_to_section(addr, segment)) == NULL) {
      fprintf(stderr, "macho_patch_dyld_info: address 0x%llx not found in any section\n",
              (uint64_t) addr);
      return -1;
   }

   if ((sectwr->section.flags & SECTION_TYPE) == S_REGULAR) {
      /*   get pointee pointer value */
      void *ptr = (char *) sectwr->data + (addr - sectwr->section.addr);
      MACHO_ADDR_T val = * (MACHO_ADDR_T *) ptr;
      
      /*   rebase value (pointer) */
      MACHO_ADDR_T newval;
      if ((newval = macho_patch_address(val, archive)) == MACHO_BAD_ADDR) {
         fprintf(stderr,
                 "macho_patch_dyld_info: unable to patch pointer 0x%llx at new address 0x%llx\n",
                 (uint64_t) val, (uint64_t) addr);
         return -1;
      }
               
      /*   save value */
      * (MACHO_ADDR_T *) ptr = newval;

      // DEBUG
      // printf("rebase 0x%llx\n", (uint64_t) newval);
   }

   return 0;
}

static int macho_patch_dyld_info_do_rebase(MACHO_ADDR_T addr, uint8_t type,
                                           struct SEGMENT *segment, struct ARCHIVE *archive) {
   assert(addr);
   
   switch (type) {
#if MACHO_BITS == 32
   case REBASE_TYPE_TEXT_ABSOLUTE32:      
#endif
   case REBASE_TYPE_POINTER:
      if (macho_patch_dyld_info_rebase_dst(addr, segment, archive) < 0) { return -1; }
      break;

#if MACHO_BITS == 64
   case REBASE_TYPE_TEXT_ABSOLUTE32:      
#endif
   case REBASE_TYPE_TEXT_PCREL32:
   default:
      fprintf(stderr, "macho_patch_dyld_info_do_rebase: invalid rebase opcode type 0x%x\n",
              type);
      return -1; 
   }
                 
   return 0;
}


#undef SYMTAB
#undef DYSYMTAB
#undef SEGMENT
#undef SECTION_WRAPPER
#undef ARCHIVE
#undef LOAD_COMMAND
#undef SECTION
#undef NLIST

#undef macho_patch_TEXT_address
#undef macho_patch_archive
#undef macho_patch_TEXT
#undef macho_patch_DATA
#undef macho_patch_dyld_info
#undef macho_patch_symbol_pointer
#undef macho_patch_address
#undef macho_patch_dyld_info_rebase_uleb
#undef macho_patch_dyld_info_rebase_dst
#undef macho_patch_dyld_info_do_rebase
#undef macho_patch_rebase_info
#undef macho_patch_bind_info
#undef macho_patch_uleb

#undef macho_find_segment
#undef macho_index_segment
#undef macho_vmaddr_to_section

#undef MACHO_SIZE_T
#undef MACHO_ADDR_T

#undef MACHO_BITS
