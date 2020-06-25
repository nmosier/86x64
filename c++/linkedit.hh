#include <mach-o/loader.h>
#include <cstdio>
#include <vector>

#include "macho-fwd.hh"
#include "image.hh"
#include "util.hh"
#include "lc.hh"

namespace MachO {

   template <Bits bits>
   class LinkeditData: public LoadCommand<bits> {
   public:
      linkedit_data_command linkedit;

      virtual uint32_t cmd() const override { return linkedit.cmd; }            
      virtual std::size_t size() const override { return sizeof(linkedit); }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual std::size_t content_size() const = 0;
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      static LinkeditData<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
             
   protected:
      LinkeditData(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         linkedit(img.at<linkedit_data_command>(offset)) {}
      virtual const void *raw_data() const { return nullptr; }
   };

   template <Bits bits>
   class DataInCode: public LinkeditData<bits> {
   public:
      std::vector<data_in_code_entry> dices;

      virtual std::size_t content_size() const override {
         return dices.size() * sizeof(data_in_code_entry);
      }
      
      template <typename... Args>
      static DataInCode<bits> *Parse(Args&&... args) { return new DataInCode<bits>(args...); }
      
      
   private:
      DataInCode(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LinkeditData<bits>(img, offset, env),
         dices(&img.at<data_in_code_entry>(this->linkedit.dataoff),
               &img.at<data_in_code_entry>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      virtual const void *raw_data() const override { return nullptr; }
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

      template <typename... Args>
      static FunctionStarts<bits> *Parse(Args&&... args) { return new FunctionStarts(args...); }
      
   private:
      FunctionStarts(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class FunctionStart {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;

      SectionBlob<bits> *function;
      
      std::size_t size() const { return sizeof(ptr_t); }
      
      template <typename... Args>
      static FunctionStart<bits> *Parse(Args&&... args) { return new FunctionStart(args...); }
      
   private:
      FunctionStart(const Image& img, std::size_t offset, std::size_t refaddr, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class CodeSignature: public LinkeditData<bits> {
   public:
      std::vector<uint8_t> cs;

      virtual std::size_t content_size() const override { return cs.size(); }

      template <typename... Args>
      static CodeSignature<bits> *Parse(Args&&... args) { return new CodeSignature(args...); }
      
   private:
      CodeSignature(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LinkeditData<bits>(img, offset, env),
         cs(&img.at<uint8_t>(this->linkedit.dataoff),
            &img.at<uint8_t>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      virtual const void *raw_data() const override { return &*cs.begin(); }
   };   
   
}
