#if MACHO_BITS == 32

# define MACHO_ADDR_T     uint32_t
# define XED_MACHINE_MODE XED_MACHINE_MODE_LEGACY_32
# define XED_REG_XIP       XED_REG_EIP
# define ARCHIVE          archive_32
# define LOAD_COMMAND     load_command_32
# define SEGMENT          segment_32
# define SECTION_WRAPPER  section_wrapper_32

# define macho_insert_instruction macho_insert_instruction_32
# define macho_insert_instruction_section macho_insert_instruction_section_32

# define macho_find_segment       macho_find_segment_32
# define macho_vmaddr_to_section  macho_vmaddr_to_section_32


#else

# define MACHO_ADDR_T     uint64_t
# define XED_MACHINE_MODE XED_MACHINE_MODE_LONG_64
# define XED_REG_XIP       XED_REG_RIP
# define ARCHIVE          archive_64
# define LOAD_COMMAND     load_command_64
# define SEGMENT          segment_64
# define SECTION_WRAPPER  section_wrapper_64

# define macho_insert_instruction macho_insert_instruction_64
# define macho_insert_instruction_section macho_insert_instruction_section_64

# define macho_find_segment       macho_find_segment_64
# define macho_vmaddr_to_section  macho_vmaddr_to_section_64

#endif

#include <stdlib.h>
#include <string.h>
#include <xed-interface.h>

#include "macho-util.h"

static int macho_insert_instruction_section(const void *instbuf, size_t instlen,
                                            MACHO_ADDR_T vmaddr, struct SECTION_WRAPPER *sectwr);


/* Inserting an instruction:
 * 1. Find all sections with SOME_INSTRUCTIONS.
 * 2. Patch each instruction.
 */

static int macho_insert_instruction(const void *instbuf, size_t instlen,
                                    MACHO_ADDR_T vmaddr, struct ARCHIVE *archive) {
   /* find text segment */
   struct SEGMENT *segtext;
   if ((segtext = macho_find_segment(SEG_TEXT, archive)) == NULL) {
      fprintf(stderr, "macho_insert_instruction: __TEXT segment missing\n");
      return -1;
   }

#if 1
   /* iterate through sections */
   for (int i = 0; i < segtext->command.nsects; ++i) {
      if (macho_insert_instruction_section(instbuf, instlen, vmaddr, &segtext->sections[i]) < 0) {
         return -1;
      }
   }
#endif
   
   /* find containing section */
   struct SECTION_WRAPPER *sectwr;
   if ((sectwr = macho_vmaddr_to_section(vmaddr, segtext)) == NULL) {
      fprintf(stderr, "macho_vmaddr_to_section: no section in segment %s contains address 0x%llx\n",
              SEG_TEXT, (uint64_t) vmaddr);
      return -1;
   }

   if ((sectwr->data = reallocf(sectwr->data, sectwr->section.size + instlen)) == NULL) {
      perror("reallocf");
      return -1;
   }
   const MACHO_ADDR_T offset = vmaddr - sectwr->section.addr;
   memmove((char *) sectwr->data + offset + instlen, (char *) sectwr->data + offset,
           sectwr->section.size - offset);
   memcpy((char *) sectwr->data + offset, instbuf, instlen);

   return 0;
}

static int macho_insert_instruction_section(const void *instbuf, size_t instlen,
                                            MACHO_ADDR_T vmaddr, struct SECTION_WRAPPER *sectwr) {
   xed_state_t dstate;
   xed_error_enum_t xerr;

   xed_state_zero(&dstate);
   xed_state_init2(&dstate, XED_MACHINE_MODE, XED_ADDRESS_WIDTH_32b);

   uint8_t *begin = sectwr->data;
   const uint8_t *end = begin + sectwr->section.size;
   for (uint8_t *it = begin; it != end; ) {
      xed_decoded_inst_t xedd;
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      if ((xerr = xed_decode(&xedd, it, end - it)) != XED_ERROR_NONE) {
         fprintf(stderr, "macho_insert_instruction_section: xed_decode: error decoding instruction "
                 "in segment %s, section %s, offset 0x%lx\n", sectwr->section.segname,
                 sectwr->section.sectname, it - begin);
         return -1;
      }

      // const xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(&xedd);
      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);
      unsigned nmemops = xed_operand_values_number_of_memory_operands(operands);

      const uint64_t old_vmaddr = (it - begin) + sectwr->section.addr +
         xed_decoded_inst_get_length(&xedd);
      const uint64_t new_vmaddr = (old_vmaddr > vmaddr) ? old_vmaddr + instlen : old_vmaddr;

      /* Memory Accesses (%rip-relative) */
      for (unsigned i = 0; i < nmemops; ++i) {
         xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(operands, i);

         /* only patch memory accesses through %rip */
         if (basereg == XED_REG_XIP && xed_operand_values_has_memory_displacement(operands)) {
            const xed_int64_t old_memdisp = xed_decoded_inst_get_memory_displacement(operands, i);
            const uint64_t old_target = old_vmaddr + old_memdisp;
            const uint64_t new_target = (old_target >= vmaddr) ? old_target + instlen : old_target;
            const xed_int64_t new_memdisp = new_target - new_vmaddr;

            const xed_enc_displacement_t disp =
               {.displacement = new_memdisp,
                .displacement_bits = xed_decoded_inst_get_memory_displacement_width_bits(operands,
                                                                                         i)
               };

            if (!xed_patch_disp(&xedd, it, disp)) {
               fprintf(stderr, "xed_patch_disp: failed to patch instruction (segment %s, section %s, offset 0x%lx)\n", sectwr->section.segname, sectwr->section.sectname, it - begin);
               return -1;
            }
         }
         
      }

      /* Jumps (%rip-relative) */
      if (xed_operand_values_has_branch_displacement(operands)) {
         const xed_int32_t old_disp = xed_decoded_inst_get_branch_displacement(operands);
         const MACHO_ADDR_T old_target = old_vmaddr + old_disp;
         const MACHO_ADDR_T new_target = (old_target >= vmaddr) ? old_target + instlen : old_target;
         const xed_int64_t new_disp = (xed_int32_t) (new_target - new_vmaddr);
         const xed_encoder_operand_t disp =
            {.type = XED_ENCODER_OPERAND_TYPE_BRDISP,
             .u = {.brdisp = new_disp},
             .width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd)
            };
         if (!xed_patch_relbr(&xedd, it, disp)) {
            fprintf(stderr, "xed_patch_relbr: failed to patch instruction "
                    "(segment %s, section %s, offset 0x%lx\n", sectwr->section.segname,
                    sectwr->section.sectname, it - begin);
            return -1;
         }
      }

      it += xed_decoded_inst_get_length(&xedd);
      
   }

   return 0;
}


#undef MACHO_BITS

#undef MACHO_ADDR_T
#undef XED_MACHINE_MODE
#undef XED_REG_XIP
#undef ARCHIVE
#undef LOAD_COMMAND
#undef SEGMENT
#undef SECTION_WRAPPER

#undef macho_insert_instruction
#undef macho_insert_instruction_section

#undef macho_find_segment
#undef macho_vmaddr_to_section
