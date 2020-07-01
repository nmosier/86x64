#include "data_in_code.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob

namespace MachO {

   template <Bits bits>
   void DataInCodeEntry<bits>::Emit(Image& img, std::size_t offset) const {
      data_in_code_entry entry = this->entry;
      entry.offset = start->loc.offset;
      img.at<data_in_code_entry>(offset) = entry;
   }

   template <Bits bits>
   DataInCodeEntry<bits>::DataInCodeEntry(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env):
      entry(img.at<data_in_code_entry>(offset))
   {
      env.offset_resolver.resolve(entry.offset, &start);
      env.data_in_code.insert(entry.offset, entry.length);
   }

   template <Bits bits>
   DataInCodeEntry<bits>::DataInCodeEntry(const DataInCodeEntry<opposite<bits>>& other,
                                          TransformEnv<opposite<bits>>& env): entry(other.entry) {
      env.resolve(other.start, &start);
   }

   template <Bits bits>
   DataInCode<bits>::DataInCode(const DataInCode<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env): LinkeditData<bits>(other, env) {
      for (auto elem : other.content) {
         content.push_back(elem->Transform(env));
      }
   }

   template <Bits bits>
   DataInCode<bits>::DataInCode(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditData<bits>(img, offset, env)
   {
      const std::size_t begin = this->linkedit.dataoff;
      const std::size_t end = begin + this->linkedit.datasize;
      for (std::size_t it = begin; it < end; it += DataInCodeEntry<bits>::size()) {
         content.push_back(DataInCodeEntry<bits>::Parse(img, it, env));
      }
   }

   template <Bits bits>
   void DataInCode<bits>::Emit_content(Image& img, std::size_t offset) const {
      for (auto elem : content) {
         elem->Emit(img, offset);
         offset += DataInCodeEntry<bits>::size();
      }
   }
   
   
   template class DataInCode<Bits::M32>;
   template class DataInCode<Bits::M64>;
}
