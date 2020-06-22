#include "linkedit.hh"
#include "macho.hh"

namespace MachO {

   template <Bits bits>
   LinkeditData<bits> *LinkeditData<bits>::Parse(const Image& img, std::size_t offset) {
      const linkedit_data_command& linkedit = img.at<linkedit_data_command>(offset);
      switch (linkedit.cmd) {
      case LC_DATA_IN_CODE:
         return DataInCode<bits>::Parse(img, offset);

      case LC_FUNCTION_STARTS:
         return FunctionStarts<bits>::Parse(img, offset);

      case LC_CODE_SIGNATURE:
         return CodeSignature<bits>::Parse(img, offset);
         
      default:
         throw error("%s: unknown linkedit command type 0x%x", __FUNCTION__, linkedit.cmd);
      }
   }
   
   template <Bits bits>
   void LinkeditData<bits>::Build(BuildEnv<bits>& env) {
#warning TODO
   }
   
   template class LinkeditData<Bits::M32>;
   template class LinkeditData<Bits::M64>;

}
