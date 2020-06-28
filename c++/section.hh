#pragma once

#include <list>
#include <vector>
#include <mach-o/loader.h>
extern "C" {
#include <xed-interface.h>
}

#include "macho-fwd.hh"
#include "util.hh"
#include "loc.hh"
#include "parse.hh"
#include "build.hh"
#include "image.hh"

namespace MachO {

   template <Bits bits> class ZeroBlob;
   template <Bits bits> class LazySymbolPointer;
   
   template <Bits bits>
   class Section {
   public:
      section_t<bits> sect;

      std::string name() const;
      Location loc() const { return Location(sect.offset, sect.addr); }
      void loc(const Location& loc) { sect.offset = loc.offset; sect.addr = loc.vmaddr; }

      virtual ~Section() {}
      
      static std::size_t size() { return sizeof(section_t<bits>); }
      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      virtual void Build(BuildEnv<bits>& env);
      virtual std::size_t content_size() const = 0;
      void Emit(Image& img, std::size_t offset) const;
      virtual void Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob) = 0;

      virtual Section<opposite<bits>> *Transform(TransformEnv<bits>& env) const = 0;

   protected:
      Section(const Image& img, std::size_t offset): sect(img.at<section_t<bits>>(offset)) {}
      Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env) {
         env(other.sect, sect);
      }
      
      virtual void Build_content(BuildEnv<bits>& env) = 0;
      virtual void Emit_content(Image& img, std::size_t offset) const = 0;
   };

   template <Bits bits, template <Bits> typename Elem>
   class SectionT: public Section<bits> {
   public:
      // static_assert(std::is_base_of<SectionT<bits, Elem, Self>, Self>());
      using Content = std::vector<Elem<bits> *>;
      Content content;

      ~SectionT();
      virtual std::size_t content_size() const override;
      virtual void Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob) override;

      static SectionT<bits, Elem> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new SectionT(img, offset, env); }

      SectionT<opposite<bits>, Elem> *Transform(TransformEnv<bits>& env) const override {
         return new SectionT<opposite<bits>, Elem>(*this, env);
      }

   protected:
      typedef Elem<bits> *(*Parser)(const Image&, const Location&, ParseEnv<bits>&);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT(img, offset, env,
                  [](const Image& img, const Location& loc, ParseEnv<bits>& env) {
                     return Elem<bits>::Parse(img, loc, env);
                  }) {}
      SectionT(const SectionT<opposite<bits>, Elem>& other, TransformEnv<opposite<bits>>& env);
      virtual void Build_content(BuildEnv<bits>& env) override;
      virtual void Emit_content(Image& img, std::size_t offset) const override;
      template <Bits, template <Bits> typename> friend class SectionT;
      
   };

   template <Bits bits>
   class TextSection: public SectionT<bits, SectionBlob> {
   public:
      template <typename... Args>
      static TextSection<bits> *Parse(Args&&... args) { return new TextSection(args...); }

      virtual TextSection<opposite<bits>> *Transform(TransformEnv<bits>& env) const override
      { return new TextSection<opposite<bits>>(*this, env); }
      
   private:
      static SectionBlob<bits> *BlobParser(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env);
      TextSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, SectionBlob>(img, offset, env, BlobParser) {}
      TextSection(const TextSection<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         SectionT<bits, SectionBlob>(other, env) {}
      template <Bits> friend class TextSection;
   };

   template <Bits bits>
   class ZerofillSection: public SectionT<bits, ZeroBlob> {
   public:
      template <typename... Args>
      static ZerofillSection<bits> *Parse(Args&&... args) { return new ZerofillSection(args...); }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual ZerofillSection<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new ZerofillSection<opposite<bits>>(*this, env);
      }
   private:
      ZerofillSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, ZeroBlob>(img, offset, env) {}
      ZerofillSection(const ZerofillSection<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env):
         SectionT<bits, ZeroBlob>(other, env) {}
      virtual void Emit_content(Image& img, std::size_t offset) const override {}

      template <Bits b> friend class ZerofillSection;
   };

   template <Bits bits>
   class SectionBlob {
   public:
      const Segment<bits> *segment; /*!< containing segment */
      Location loc; /*!< Post-build location */
      
      virtual std::size_t size() const = 0;
      virtual ~SectionBlob() {}
      void Build(BuildEnv<bits>& env);
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
      template <typename... Args>
      static DataBlob<bits> *Parse(Args&&... args) { return new DataBlob(args...); }
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

      static ZeroBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
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
   using DataSection = SectionT<bits, DataBlob>;

   template <Bits bits>
   using LazySymbolPointerSection = SectionT<bits, LazySymbolPointer>;

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

      static LazySymbolPointer<bits> *Parse(const Image& img, const Location& loc,
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
      static NonLazySymbolPointer<bits> *Parse(const Image& img, const Location& loc,
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
   class NonLazySymbolPointerSection: public SectionT<bits, NonLazySymbolPointer> {
   public:
      template <typename... Args>
      static NonLazySymbolPointerSection<bits> *Parse(Args&&... args) {
         return new NonLazySymbolPointerSection(args...);
      }
      
   private:
      NonLazySymbolPointerSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, NonLazySymbolPointer>(img, offset, env) {}
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
      
      static Immediate<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env) {
         return new Immediate(img, loc, env);
      }

      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual Immediate<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Immediate<opposite<bits>>(*this, env);
      }
      
   private:
      Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(img, loc, env), value(img.at<uint32_t>(loc.offset)), pointee(nullptr) {}
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
      InstructionPointer<bits> *memptr; /*!< memory pointer */
      const SectionBlob<bits> *brdisp;  /*!< branch displacement pointee */
      
      virtual std::size_t size() const override { return instbuf.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static Instruction<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env) {
         return new Instruction(img, loc, env);
      }
      
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
