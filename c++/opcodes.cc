#include "opcodes.hh"
#include "section_blob.hh"

namespace MachO::opcode {

   namespace {

      template <typename It, typename T>
      It encode_int(It it, T val) {
         static_assert(std::is_integral<T>());
         for (std::size_t i = 0; i < sizeof(val); ++i, val >>= 8) {
            *it++ = val & 0xff;
         }
         return it;
      }

      template <typename T>
      void encode_int(opcode_t& opcode, T val) {
         encode_int(std::back_inserter(opcode), val);
      }
      
   }
   
   opcode_t lea_r11_mem_rip_disp32(int32_t disp) {
      opcode_t opcode {0x4c, 0x8d, 0x1d};
      encode_int(opcode, disp);
      return opcode;
   }

   template <Bits bits>
   int32_t disp32(const SectionBlob<bits> *src, const SectionBlob<bits> *dst,
                  std::size_t new_src_len) {
      const std::size_t dst_vmaddr = dst->loc.vmaddr;
      const std::size_t src_vmaddr = src->loc.vmaddr;
      const std::size_t src_size   = src->size();

      if (dst_vmaddr > src_vmaddr) {
         return dst_vmaddr - (src_vmaddr + src_size);
      } else {
         return dst_vmaddr - (src_vmaddr + new_src_len);
      }
   }

#if 0
   int32_t disp32(const SectionBlob<Bits::M32> *src, const SectionBlob<Bits::M32> *dst,
                  std::size_t new_src_len);
   int32_t disp32(const SectionBlob<Bits::M64> *src, const SectionBlob<Bits::M64> *dst,
                  std::size_t new_src_len);
#endif
   
   opcode_t lea_r11_mem_rip_disp32(const SectionBlob<Bits::M32> *src,
                                   const SectionBlob<Bits::M32> *dst) {
      opcode_t opcode {0x4c, 0x8d, 0x1d};

      /* compute offset */
      const std::size_t new_src_size = opcode.size() + sizeof(int32_t);
      const int32_t disp = disp32(src, dst, new_src_size);

      encode_int(opcode, disp);
      return opcode;
   }
   
   
}
