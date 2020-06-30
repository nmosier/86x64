#include "linkedit.hh"
#include "macho.hh"
#include "leb.hh"
#include "segment.hh"

namespace MachO {

   template <Bits bits>
   LinkeditData<bits> *LinkeditData<bits>::Parse(const Image& img, std::size_t offset,
                                                 ParseEnv<bits>& env) {
      const linkedit_data_command& linkedit = img.at<linkedit_data_command>(offset);
      switch (linkedit.cmd) {
      case LC_DATA_IN_CODE:
         return DataInCode<bits>::Parse(img, offset, env);

      case LC_FUNCTION_STARTS:
         return FunctionStarts<bits>::Parse(img, offset, env);

      case LC_CODE_SIGNATURE:
         return CodeSignature<bits>::Parse(img, offset, env);
         
      default:
         throw error("%s: unknown linkedit command type 0x%x", __FUNCTION__, linkedit.cmd);
      }
   }
   
   template <Bits bits>
   void LinkeditData<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      linkedit.datasize = this->content_size();
      linkedit.dataoff = env.allocate(linkedit.datasize);
      assert(linkedit.datasize % sizeof(ptr_t<bits>) == 0);
   }

   template <Bits bits>
   void LinkeditData<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<linkedit_data_command>(offset) = linkedit;
      img.copy(linkedit.dataoff, (const char *) raw_data(), linkedit.datasize);
   }

   template <Bits bits>
   FunctionStarts<bits>::FunctionStarts(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditData<bits>(img, offset, env), segment(env.archive.segment(SEG_TEXT))
   {
      const std::size_t begin = this->linkedit.dataoff;
      const std::size_t end = begin + this->linkedit.datasize;
      std::size_t it = begin;
      std::size_t refaddr = segment->loc().offset;

      while (it != end) {
         std::size_t uleb;
         it += leb128_decode(img, it, uleb);
         // it += leb128_decode(&img.at<uint8_t>(it), img.size() - it, uleb);
         if (uleb != 0 || refaddr == segment->loc().offset) {
            entries.push_back(nullptr);
            refaddr += uleb;
            env.offset_resolver.resolve(refaddr, &entries.back());
         }
      }
   }

   template <Bits bits>
   std::size_t FunctionStarts<bits>::content_size() const {
      std::size_t size = 0;
      std::size_t refaddr = segment->loc().offset;
      
      for (const SectionBlob<bits> *entry : entries) {
         size += leb128_size(entry->loc.offset - refaddr);
         refaddr = entry->loc.offset;
      }
      
      return align<bits>(size);
   }

   template <Bits bits>
   void FunctionStarts<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<linkedit_data_command>(offset) = this->linkedit;

      std::size_t refaddr = segment->loc().offset;
      const std::size_t begin = this->linkedit.dataoff;
      const std::size_t size = this->linkedit.datasize;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      for (const SectionBlob<bits> *entry : entries) {
         it += leb128_encode(img, it, entry->loc.offset - refaddr);
         refaddr = entry->loc.offset;
      }
      img.memset(it, 0, end - it);
   }

   template <Bits bits>
   FunctionStarts<bits>::FunctionStarts(const FunctionStarts<opposite<bits>>& other,
                                        TransformEnv<opposite<bits>>& env):
      LinkeditData<bits>(other, env)
   {
      for (const auto entry : other.entries) {
         entries.push_back(entry->Transform(env));
      }
      env.resolve(other.segment, &segment);
   }

   template class LinkeditData<Bits::M32>;
   template class LinkeditData<Bits::M64>;

}
