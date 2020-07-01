#pragma once

#include <list>
#include <vector>
#include <mach-o/loader.h>
extern "C" {
#include <xed-interface.h>
}

#include "util.hh"
#include "loc.hh"
#include "image.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class Section {
   public:
      using Content = std::list<SectionBlob<bits> *>;
      
      section_t<bits> sect;
      Content content;

      std::string name() const;
      Location loc() const { return Location(sect.offset, sect.addr); }
      void loc(const Location& loc) { sect.offset = loc.offset; sect.addr = loc.vmaddr; }

      virtual ~Section();
      
      static std::size_t size() { return sizeof(section_t<bits>); }

      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      void Parse1(const Image& img, ParseEnv<bits>& env);
      void Parse2(const Image& img, ParseEnv<bits>& env);

      virtual void Build(BuildEnv<bits>& env);
      std::size_t content_size() const;
      void Emit(Image& img, std::size_t offset) const;
      void Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob);

      virtual Section<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Section<opposite<bits>>(*this, env);
      }

      typename Content::iterator find(std::size_t vmaddr); /* inclusive greatest lower bound */
      typename Content::const_iterator find(std::size_t vmaddr) const;

   protected:
      typedef SectionBlob<bits> *(*Parser)(const Image&, const Location&, ParseEnv<bits>&);
      Parser parser;
      
      Section(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser):
         sect(img.at<section_t<bits>>(offset)), parser(parser) {}
      Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      static SectionBlob<bits> *TextParser(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env);

      template <Bits> friend class Section;
   };

   template <Bits bits>
   class SectionBlob {
   public:
      const Segment<bits> *segment; /*!< containing segment */
      Location loc; /*!< Post-build location, also used during parsing */
      
      virtual std::size_t size() const = 0;
      virtual ~SectionBlob() {}
      virtual void Build(BuildEnv<bits>& env);
      virtual void Emit(Image& img, std::size_t offset) const = 0;
      virtual SectionBlob<opposite<bits>> *Transform(TransformEnv<bits>& env) const = 0;
      
   protected:
      SectionBlob(const Location& loc, ParseEnv<bits>& env);
      SectionBlob(Segment<bits> *segment): segment(segment) {}
      SectionBlob(const SectionBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      uint8_t data;
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env) {
         return new DataBlob(img, loc, env);
      }
      
      virtual DataBlob<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DataBlob<opposite<bits>>(*this, env);
      }
      
   private:
      DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(loc, env), data(img.at<uint8_t>(loc.offset)) {}
      DataBlob(const DataBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         SectionBlob<bits>(other, env), data(other.data) {}

      template <Bits b> friend class DataBlob;
   };

   template <Bits bits>
   class ZeroBlob: public SectionBlob<bits> {
   public:
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override {}

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new ZeroBlob(img, loc, env); }
      virtual ZeroBlob<opposite<bits>> *Transform(TransformEnv<bits>& env) const override
      { return new ZeroBlob<opposite<bits>>(*this, env); }
      
   private:
      ZeroBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(loc, env) {}
      ZeroBlob(const ZeroBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         SectionBlob<bits>(other, env) {}
      
      template <Bits b> friend class ZeroBlob;
   };

   template <Bits bits>
   class SymbolPointer: public SectionBlob<bits> {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      static std::size_t Size() { return sizeof(ptr_t); }
      virtual std::size_t size() const override { return sizeof(ptr_t); }
      virtual void Emit(Image& img, std::size_t offset) const override;
      
   protected:
      template <typename... Args>
      SymbolPointer(Args&&... args): SectionBlob<bits>(args...) {}
      virtual ptr_t raw_data() const = 0;
   };

   template <Bits bits>
   class LazySymbolPointer: public SymbolPointer<bits> {
   public:
      const Instruction<bits> *pointee; /*!< initial pointee */

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc,
                                            ParseEnv<bits>& env) {
         return new LazySymbolPointer(img, loc, env);
      }

      virtual LazySymbolPointer<opposite<bits>> *Transform(TransformEnv<bits>& env) const override
      {
         return new LazySymbolPointer<opposite<bits>>(*this, env);
      }
     
   private:
      LazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env);
      LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                        TransformEnv<opposite<bits>>& env);
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override {
         return pointee->loc.vmaddr;
      }
      template <Bits> friend class LazySymbolPointer;
   };

   template <Bits bits>
   class NonLazySymbolPointer: public SymbolPointer<bits> {
   public:
      static SectionBlob<bits> *Parse(const Image& img, const Location& loc,
                                               ParseEnv<bits>& env) {
         return new NonLazySymbolPointer(img, loc, env);
      }

      virtual NonLazySymbolPointer<opposite<bits>> *Transform(TransformEnv<bits>& env) const
         override { return new NonLazySymbolPointer<opposite<bits>>(*this, env); }
      
   private:
      NonLazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SymbolPointer<bits>(loc, env) {}
      NonLazySymbolPointer(const NonLazySymbolPointer<opposite<bits>>& other,
                           TransformEnv<opposite<bits>>& env):
         SymbolPointer<bits>(other, env) {}
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override { return 0x0; }
      template <Bits> friend class NonLazySymbolPointer;
   };

   template <Bits bits>
   class InstructionPointer: public SectionBlob<bits> {
   public:
      const SectionBlob<bits> *pointee;
      virtual std::size_t size() const override { return bits == Bits::M32 ? 4 : 8; }

      static InstructionPointer<bits> *Parse(const Image& img, const Location& loc,
                                             ParseEnv<bits>& env)
      { return new InstructionPointer(img, loc, env); }
      
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual InstructionPointer<opposite<bits>> *Transform(TransformEnv<bits>& env) const override
      { return new InstructionPointer<opposite<bits>>(*this, env); }
      
   private:
      InstructionPointer(const Image& img, const Location& loc, ParseEnv<bits>& env);
      InstructionPointer(const InstructionPointer<opposite<bits>>& other,
                         TransformEnv<opposite<bits>>& env);
      template <Bits> friend class InstructionPointer;
   };

   template <Bits bits>
   class Immediate: public SectionBlob<bits> {
   public:
      uint32_t value;
      const SectionBlob<bits> *pointee; /*!< optional -- only if deemed to be pointer */
      virtual std::size_t size() const override { return sizeof(uint32_t); }
      
      static Immediate<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                    bool is_pointer)
      { return new Immediate(img, loc, env, is_pointer); }
      
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual Immediate<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Immediate<opposite<bits>>(*this, env);
      }
      
   private:
      Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env, bool is_pointer);
      Immediate(const Immediate<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class Immediate;
   };
   
   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t dstate;
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      unsigned memidx;
      const SectionBlob<bits> *memdisp; /*!< memory displacement pointee */
      Immediate<bits> *imm;
      const SectionBlob<bits> *brdisp;  /*!< branch displacement pointee */
      
      virtual std::size_t size() const override { return instbuf.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual void Build(BuildEnv<bits>& env) override;

      static Instruction<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new Instruction(img, loc, env); }
      
      template <typename It>
      Instruction(Segment<bits> *segment, It begin, It end):
         SectionBlob<bits>(segment), instbuf(begin, end), memdisp(nullptr), brdisp(nullptr) {}

      Instruction<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Instruction<opposite<bits>>(*this, env);
      }

   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env);
      Instruction(const Instruction<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class Instruction;
   };

   template <Bits bits>
   class EntryPointBlob: public SectionBlob<bits> {
   public:
      template <typename... Args>
      EntryPointBlob<bits> *Create(Args&&... args) { return new EntryPointBlob(args...); }

      virtual std::size_t size() const override { return 0; }
      virtual void Emit(Image& img, std::size_t offset) const override {}
      
   private:
      template <typename... Args>
      EntryPointBlob(Args&&... args): SectionBlob<bits>(args...) {}
   };

}
