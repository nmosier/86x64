#include <mach-o/loader.h>
#include <cstdio>
#include <vector>

#include "macho-fwd.hh"
#include "image.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   class LinkeditData: public LoadCommand<bits> {
   public:
      linkedit_data_command linkedit;

      virtual uint32_t cmd() const override { return linkedit.cmd; }            
      virtual std::size_t size() const override { return sizeof(linkedit); }
      
      static LinkeditData<bits> *Parse(const Image& img, std::size_t offset);
      
   protected:
      LinkeditData(const Image& img, std::size_t offset):
         linkedit(img.at<linkedit_data_command>(offset)) {}
   };

   template <Bits bits>
   class DataInCode: public LinkeditData<bits> {
   public:
      std::vector<data_in_code_entry> dices;

      template <typename... Args>
      static DataInCode<bits> *Parse(Args&&... args) { return new DataInCode<bits>(args...); }
      
   private:
      DataInCode(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         dices(&img.at<data_in_code_entry>(this->linkedit.dataoff),
               &img.at<data_in_code_entry>(this->linkedit.dataoff + this->linkedit.datasize)) {}
   };

   template <Bits bits>
   class FunctionStarts: public LinkeditData<bits> {
   public:
      using pointer_t = select_type<bits, uint32_t, uint64_t>;
      using Entries = std::vector<pointer_t>;

      Entries function_starts;

      template <typename... Args>
      static FunctionStarts<bits> *Parse(Args&&... args) { return new FunctionStarts(args...); }
      
   private:
      FunctionStarts(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         function_starts(&img.at<pointer_t>(this->linkedit.dataoff),
                         &img.at<pointer_t>(this->linkedit.dataoff + this->linkedit.datasize))
      {}
   };

   template <Bits bits>
   class CodeSignature: public LinkeditData<bits> {
   public:
      std::vector<uint8_t> cs;

      template <typename... Args>
      static CodeSignature<bits> *Parse(Args&&... args) { return new CodeSignature(args...); }
      
   private:
      CodeSignature(const Image& img, std::size_t offset):
         LinkeditData<bits>(img, offset),
         cs(&img.at<uint8_t>(this->linkedit.dataoff),
            &img.at<uint8_t>(this->linkedit.dataoff + this->linkedit.datasize)) {}
   };   
   
}
