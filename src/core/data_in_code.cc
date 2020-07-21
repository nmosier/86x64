#include "data_in_code.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob
#include "segment.hh" // Segment

namespace MachO {

   template <Bits bits>
   void DataInCodeEntry<bits>::Emit(Image& img, std::size_t offset) const {
      const std::size_t begin_off = start->loc.offset;
      std::size_t end_off;
      if (end) {
         end_off = end->loc.offset;
      } else {
         /* assume that this data in code section runs to the end */
         /* find containing section */
         for (auto section : start->segment->sections) {
            if (begin_off >= section->sect.offset &&
                begin_off < section->sect.offset + section->sect.addr) {
               end_off = section->sect.offset + section->sect.addr;
               break;
            }
         }
      }
      
      data_in_code_entry entry = this->entry;
      entry.offset = begin_off;
      entry.length = end_off - begin_off;
      img.at<data_in_code_entry>(offset) = entry;
   }

   template <Bits bits>
   DataInCodeEntry<bits>::DataInCodeEntry(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env):
      entry(img.at<data_in_code_entry>(offset))
   {
      env.offset_resolver.resolve(entry.offset, &start);
      env.offset_resolver.resolve(entry.offset + entry.length, &end);
      env.data_in_code.insert(entry.offset, entry.length);
   }

   template <Bits bits>
   DataInCodeEntry<bits>::DataInCodeEntry(const DataInCodeEntry<opposite<bits>>& other,
                                          TransformEnv<opposite<bits>>& env): entry(other.entry) {
      env.resolve(other.start, &start);
      env.resolve(other.end, &end);
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
