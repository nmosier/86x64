#pragma once

#include <cstdlib>
#include <sstream>

#include "types.hh"
#include "linkedit.hh"

namespace MachO {

   template <Bits bits>
   class DataInCodeEntry {
   public:
#warning DataInCodeEntry not complete
      data_in_code_entry entry;
      const SectionBlob<bits> *start = nullptr; /*!< first data entry */
      const SectionBlob<bits> *end = nullptr; /*!< last data entry */

      void Emit(Image& img, std::size_t offset) const;
      
      static std::size_t size() { return sizeof(data_in_code_entry); }
      static DataInCodeEntry<bits> *Parse(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env) {
         return new DataInCodeEntry(img, offset, env);
      }
      DataInCodeEntry<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new DataInCodeEntry<opposite<bits>>(*this, env);
      }
      
   protected:
      DataInCodeEntry(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DataInCodeEntry(const DataInCodeEntry<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env);
      template <Bits> friend class DataInCodeEntry;
   };

   template <Bits bits>
   class DataInCode: public LinkeditData<bits> {
   public:
      using Content = std::vector<DataInCodeEntry<bits> *>;
      Content content;

      virtual std::size_t content_size() const override {
         return content.size() * sizeof(DataInCodeEntry<bits>::size());
      }
      
      static DataInCode<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DataInCode(img, offset, env);
      }
      
      virtual DataInCode<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DataInCode<opposite<bits>>(*this, env);
      }

   private:
      DataInCode(const DataInCode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      DataInCode(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      
      virtual void Emit_content(Image& img, std::size_t offset) const override;

      template <Bits> friend class DataInCode;
   };   

}
