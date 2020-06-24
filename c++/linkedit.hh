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
      
      static LinkeditData<bits> *Parse(const Image& img, std::size_t offset);
             
   protected:
      LinkeditData(const Image& img, std::size_t offset):
         linkedit(img.at<linkedit_data_command>(offset)) {}
      virtual const void *raw_data() const = 0;
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
      DataInCode(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         dices(&img.at<data_in_code_entry>(this->linkedit.dataoff),
               &img.at<data_in_code_entry>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      virtual const void *raw_data() const override { return &*dices.begin(); }
   };

   template <Bits bits>
   class FunctionStarts: public LinkeditData<bits> {
   public:
      using pointer_t = select_type<bits, uint32_t, uint64_t>;
      using Entries = std::vector<pointer_t>;

      Entries function_starts;

      virtual std::size_t content_size() const override {
         return function_starts.size() * sizeof(pointer_t);
      }

      template <typename... Args>
      static FunctionStarts<bits> *Parse(Args&&... args) { return new FunctionStarts(args...); }
      
   private:
      FunctionStarts(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         function_starts(&img.at<pointer_t>(this->linkedit.dataoff),
                         &img.at<pointer_t>(this->linkedit.dataoff + this->linkedit.datasize))
      {}

      virtual const void *raw_data() const override { return &*function_starts.begin(); }
   };

   template <Bits bits>
   class CodeSignature: public LinkeditData<bits> {
   public:
      std::vector<uint8_t> cs;

      virtual std::size_t content_size() const override { return cs.size(); }

      template <typename... Args>
      static CodeSignature<bits> *Parse(Args&&... args) { return new CodeSignature(args...); }
      
   private:
      CodeSignature(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         cs(&img.at<uint8_t>(this->linkedit.dataoff),
            &img.at<uint8_t>(this->linkedit.dataoff + this->linkedit.datasize)) {}
      virtual const void *raw_data() const override { return &*cs.begin(); }
   };   
   
}
