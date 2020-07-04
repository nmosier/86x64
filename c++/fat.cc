#include "fat.hh"
#include "archive.hh"
#include "error.hh"

namespace MachO {

   template <Bits bits>
   Fat<bits>::Fat(const Image& img): header(img.at<fat_header>(0)) {
      std::size_t offset = sizeof(fat_header);
      for (uint32_t i = 0; i < header.nfat_arch; ++i) {
         fat_arch_t<bits> arch = img.at<fat_arch_t<bits>>(offset);
         
         try {
            archives.emplace_back(arch, AbstractArchive::Parse(img, arch.offset));
         } catch (const bad_format& err) {
            std::cerr << "warning: archive " << i << ": " << err.what() << std::endl;
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
      img.at<fat_header>(0) = header;
      offset += sizeof(header);
      
      for (const ArchiveNode& archive : archives) {
         img.at<fat_arch_t<bits>>(offset) = archive.first;
         offset += sizeof(fat_arch_t<bits>);
         archive.second->Emit(img);
      }
   }
   
   template class Fat<Bits::M32>;
   template class Fat<Bits::M64>;

}
