#include <mach-o/loader.h>
#include <cstdio>
#include <vector>

#include "macho-fwd.hh"
#include "image.hh"
#include "util.hh"
#include "lc.hh"

namespace MachO {

   template <Bits bits>
   class LinkeditData: public LinkeditCommand<bits> {
   public:
      linkedit_data_command linkedit;
      
      virtual uint32_t cmd() const override { return linkedit.cmd; }            
      virtual std::size_t size() const override { return sizeof(linkedit); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      static LinkeditData<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
             
   protected:
      LinkeditData(const LinkeditData<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env), linkedit(other.linkedit) {}
      LinkeditData(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LinkeditCommand<bits>(img, offset, env), linkedit(img.at<linkedit_data_command>(offset))
      {}
      virtual const void *raw_data() const { return nullptr; }
   };

   template <Bits bits>
   class DataInCodeEntry {
   public:
#warning DataInCodeEntry not complete
      data_in_code_entry entry;
      SectionBlob<bits> *start = nullptr;

      void Build(BuildEnv<bits>& build) {}
      void Emit(Image& img, std::size_t offset) const {
         data_in_code_entry entry = this->entry;
         entry.offset = start->loc.offset;
         img.at<data_in_code_entry>(offset) = entry;
      }
      
      static std::size_t size() { return sizeof(data_in_code_entry); }
      static DataInCodeEntry<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new DataInCodeEntry(img, offset, env); }
      DataInCodeEntry<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new DataInCodeEntry<opposite<bits>>(*this, env);
      }
      
   protected:
      DataInCodeEntry(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         entry(img.at<data_in_code_entry>(offset))
      {
         env.offset_resolver.resolve(entry.offset, &start);
      }
      DataInCodeEntry(const DataInCodeEntry<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env): entry(other.entry) {
         env.resolve(other.start, &start);
      }
      template <Bits> friend class DataInCodeEntry;
   };

   template <Bits bits>
   class DataInCode: public LinkeditData<bits> {
   public:
      std::vector<data_in_code_entry> dices;

      virtual std::size_t content_size() const override {
         return dices.size() * sizeof(data_in_code_entry);
      }

      static DataInCode<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DataInCode(img, offset, env);
      }
      
      virtual DataInCode<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DataInCode<opposite<bits>>(*this, env);
      }

   private:
      DataInCode(const DataInCode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditData<bits>(other, env), dices(other.dices) {}
      DataInCode(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LinkeditData<bits>(img, offset, env),
         dices(&img.at<data_in_code_entry>(this->linkedit.dataoff),
               &img.at<data_in_code_entry>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      virtual const void *raw_data() const override { return dices.data(); }

      template <Bits b> friend class DataInCode;
   };
   

   template <Bits bits>
   class FunctionStarts: public LinkeditData<bits> {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Entries = std::list<const SectionBlob<bits> *>;

      Entries entries;
      const Segment<bits> *segment;

      virtual std::size_t content_size() const override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static FunctionStarts<bits> *Parse(const Image& img, std::size_t offset,
                                         ParseEnv<bits>& env)
      { return new FunctionStarts(img, offset, env); }
      
      virtual FunctionStarts<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new FunctionStarts<opposite<bits>>(*this, env);
      }

   private:
      FunctionStarts(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      FunctionStarts(const FunctionStarts<opposite<bits>>& other,
                     TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class FunctionStarts;
   };

   template <Bits bits>
   class CodeSignature: public LinkeditData<bits> {
   public:
      std::vector<uint8_t> cs;

      virtual std::size_t content_size() const override { return cs.size(); }

      static CodeSignature<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new CodeSignature(img, offset, env); }
      
      virtual CodeSignature<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new CodeSignature<opposite<bits>>(*this, env);
      }

   private:
      CodeSignature(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LinkeditData<bits>(img, offset, env),
         cs(&img.at<uint8_t>(this->linkedit.dataoff),
            &img.at<uint8_t>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      CodeSignature(const CodeSignature<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditData<bits>(other, env), cs(other.cs) {}
      virtual const void *raw_data() const override { return &*cs.begin(); }

      template <Bits b> friend class CodeSignature;
   };   
   
}
