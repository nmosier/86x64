#pragma once

extern "C" {
#include <xed-interface.h>
}
#include <vector>

#include "types.hh"
#include "loc.hh"

namespace MachO {

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
      DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env);
      DataBlob(const DataBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

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
