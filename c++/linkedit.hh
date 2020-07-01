#pragma once

#include <cstdio>
#include <vector>

#include "image.hh"
#include "util.hh"
#include "lc.hh"
#include "types.hh"

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
      
      virtual void Emit_content(Image& img, std::size_t offset) const = 0;
   };

   

   template <Bits bits>
   class FunctionStarts: public LinkeditData<bits> {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Entries = std::list<const SectionBlob<bits> *>;

      Entries entries;
      const Segment<bits> *segment;

      virtual std::size_t content_size() const override;

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

      virtual void Emit_content(Image& img, std::size_t offset) const override;

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

      virtual void Emit_content(Image& img, std::size_t offset) const override;

      template <Bits b> friend class CodeSignature;
   };   
   
}
