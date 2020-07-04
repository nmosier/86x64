#include "fat.hh"
#include "archive.hh"
#include "error.hh"
#include "util.hh"

namespace MachO {

   template <>
   constexpr void big_endian(const fat_header& in, fat_header& out) {
      big_endian<uint32_t>(in.magic, out.magic);
      big_endian<uint32_t>(in.nfat_arch, out.nfat_arch);
   }

   template <>
   constexpr void big_endian(const fat_arch& in, fat_arch& out) {
      big_endian<cpu_type_t>(in.cputype, out.cputype);
      big_endian<cpu_subtype_t>(in.cpusubtype, out.cpusubtype);
      big_endian<uint32_t>(in.offset, out.offset);
      big_endian<uint32_t>(in.size, out.size);
      big_endian<uint32_t>(in.align, out.align);
   }

   template <>
   constexpr void big_endian(const fat_arch_64& in, fat_arch_64& out) {
      big_endian<cpu_type_t>(in.cputype, out.cputype);
      big_endian<cpu_subtype_t>(in.cpusubtype, out.cpusubtype);
      big_endian<uint64_t>(in.offset, out.offset);
      big_endian<uint64_t>(in.size, out.size);
      big_endian<uint32_t>(in.align, out.align);
      big_endian<uint32_t>(in.reserved, out.reserved);
   }
   
   template <Bits bits>
   Fat<bits>::Fat(const Image& img) {
      /* convert header to big endian */
      big_endian(img.at<fat_header>(0), header);
      
      std::size_t offset = sizeof(fat_header);
      for (uint32_t i = 0; i < header.nfat_arch; ++i) {
         fat_arch_t<bits> arch;
         big_endian(img.at<fat_arch_t<bits>>(offset), arch);

         if (arch.cputype == CPU_TYPE_X86 || arch.cputype == CPU_TYPE_X86_64) {
            try {
               archives.emplace_back(arch, AbstractArchive::Parse(img, arch.offset));
            } catch (const bad_format& err) {
               std::cerr << "warning: archive " << i << ": " << err.what() << std::endl;
            }
         }
         
         offset += sizeof(fat_arch_t<bits>);
      }
   }
   
   template <Bits bits>
   void Fat<bits>::Build() {
      std::size_t offset = size();
      for (ArchiveNode& archive : archives) {
         offset = align_up<std::size_t>(offset, archive.first.align);
         archive.first.offset = offset;
         offset = archive.second->Build(offset);
         archive.first.size = archive.first.offset - offset;
      }
   }
   
   template <Bits bits>
   void Fat<bits>::Emit(Image& img) const {
      std::size_t offset = 0;
      big_endian(header, img.at<fat_header>(0));
      offset += sizeof(header);
      
      for (const ArchiveNode& archive : archives) {
         big_endian(archive.first, img.at<fat_arch_t<bits>>(offset));
         offset += sizeof(fat_arch_t<bits>);
         archive.second->Emit(img);
      }
   }
   
   template class Fat<Bits::M32>;
   template class Fat<Bits::M64>;

}
