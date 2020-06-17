#include "dyldinfo.hh"
#include "image.hh"

namespace MachO {

   template <Bits bits>
   DyldInfo<bits> *DyldInfo<bits>::Parse(const Image& img, std::size_t offset) {
      DyldInfo<bits> *dyld = new DyldInfo<bits>;
      dyld->dyld_info = img.at<dyld_info_command>(offset);
      dyld->rebase = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.rebase_off),
                                          &img.at<uint8_t>(dyld->dyld_info.rebase_off +
                                                           dyld->dyld_info.rebase_size));
      dyld->bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.bind_off),
                                        &img.at<uint8_t>(dyld->dyld_info.bind_off +
                                                         dyld->dyld_info.bind_size));
      dyld->weak_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.weak_bind_off),
                                             &img.at<uint8_t>(dyld->dyld_info.weak_bind_off +
                                                              dyld->dyld_info.weak_bind_size));
      dyld->lazy_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.lazy_bind_off),
                                             &img.at<uint8_t>(dyld->dyld_info.lazy_bind_off +
                                                              dyld->dyld_info.lazy_bind_size));
      dyld->export_info = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.export_off),
                                               &img.at<uint8_t>(dyld->dyld_info.export_off +
                                                                dyld->dyld_info.export_size));
      return dyld;
   }

   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;
   
}
