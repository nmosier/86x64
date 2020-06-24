#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <optional>
#include <string>

extern "C" {
#include <xed-interface.h>
}

#include <mach-o/loader.h>

#include "image.hh"
#include "util.hh"
#include "lc.hh"
#include "parse.hh"

namespace MachO {

   template <Bits bits> class ZeroBlob;
   template <Bits bist> class LazySymbolPointer;
   
   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using segment_command_t = select_type<bits, segment_command, segment_command_64>;
      using Sections = std::vector<Section<bits> *>;

      segment_command_t segment_command;
      Sections sections;
      unsigned id; /*!< assigned during build time */

      virtual uint32_t cmd() const override { return segment_command.cmd; }
      virtual std::size_t size() const override;
      std::string name() const;
      std::size_t vmaddr() const { return segment_command.vmaddr; }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void AssignID(BuildEnv<bits>& env) override {
         id = env.template counter<decltype(this)>();
      };
      virtual void Emit(Image& img, std::size_t offset) const override;
      Location loc() const { return Location(segment_command.fileoff, segment_command.vmaddr); }

      Section<bits> *section(const std::string& name);

      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   private:
      Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      void Build_PAGEZERO(BuildEnv<bits>& env); /*!< for SEG_PAGEZERO segment */
      void Build_default(BuildEnv<bits>& env);
      void Build_LINKEDIT(BuildEnv<bits>& env); /*!< for SEG_LINKEDIT segment */
   };

#if 0
   template <Bits bits>
   class Segment_LINKEDIT: public Segment<bits> {
   public:
      virtual void Build(BuildEnv<bits>& env) override;
      
      template <typename... Args>
      static Segment_LINKEDIT<bits> *Parse(Args&&... args) { return new Segment_LINKEDIT(args...); }
      
   private:
      template <typename... Args>
      Segment_LINKEDIT(Args&&... args): Segment<bits>(args...) {}
   };
#endif
   

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;

      std::string name() const;
      Location loc() const { return Location(sect.offset, sect.addr); }
      void loc(const Location& loc) { sect.offset = loc.offset; sect.addr = loc.vmaddr; }

      virtual ~Section() {}
      
      static std::size_t size() { return sizeof(section_t); }
      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      void Build(BuildEnv<bits>& env);
      virtual std::size_t content_size() const = 0;
      void Emit(Image& img, std::size_t offset) const;

   protected:
      Section(const Image& img, std::size_t offset): sect(img.at<section_t>(offset)) {}
      virtual void Build_content(BuildEnv<bits>& env) = 0;
      virtual void Emit_content(Image& img, std::size_t offset) const = 0; 
   };

   template <Bits bits, class Elem>
   class SectionT: public Section<bits> {
   public:
      using Content = std::list<Elem *>;
      Content content;

      ~SectionT();
      virtual std::size_t content_size() const override;
      
   protected:
      typedef Elem *(*Parser)(const Image&, const Location&, ParseEnv<bits>&);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT(img, offset, env, [](const Image& img, const Location& loc, ParseEnv<bits>& env) {
                                       return Elem::Parse(img, loc, env);
                                    }) {}
      virtual void Build_content(BuildEnv<bits>& env) override;
      virtual void Emit_content(Image& img, std::size_t offset) const override;
   };

   template <Bits bits>
   class TextSection: public SectionT<bits, SectionBlob<bits>> {
   public:
      template <typename... Args>
      static TextSection<bits> *Parse(Args&&... args) { return new TextSection(args...); }
   private:
      static SectionBlob<bits> *BlobParser(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env);
      TextSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, SectionBlob<bits>>(img, offset, env, BlobParser) {}
   };

   template <Bits bits>
   class ZerofillSection: public SectionT<bits, ZeroBlob<bits>> {
   public:
      template <typename... Args>
      static ZerofillSection<bits> *Parse(Args&&... args) { return new ZerofillSection(args...); }
   private:
      ZerofillSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, ZeroBlob<bits>>(img, offset, env) {}
      virtual void Emit_content(Image& img, std::size_t offset) const override {}
   };

   template <Bits bits>
   class SectionBlob {
   public:
      Segment<bits> *segment; /*!< containing segment */
      Location loc; /*!< Post-build location */
      
      virtual std::size_t size() const = 0;
      virtual ~SectionBlob() {}
      void Build(BuildEnv<bits>& env);
      virtual void Emit(Image& img, std::size_t offset) const = 0;
      
   protected:
      SectionBlob(const Location& loc, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      uint8_t data;
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override;
      template <typename... Args>
      static DataBlob<bits> *Parse(Args&&... args) { return new DataBlob(args...); }
      
   private:
      DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(loc, env), data(img.at<uint8_t>(loc.offset)) {}
   };

   template <Bits bits>
   class ZeroBlob: public SectionBlob<bits> {
   public:
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override {}

      template <typename... Args>
      static ZeroBlob<bits> *Parse(Args&&... args) {
         return new ZeroBlob(std::forward<Args>(args)...);
      }
      
   private:
      ZeroBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(loc, env) {}
   };
   
   template <Bits bits>
   class DataSection: public SectionT<bits, DataBlob<bits>> {
   public:
      template <typename... Args>
      static DataSection<bits> *Parse(Args&&... args) {
         return new DataSection(std::forward<Args>(args)...);
      }
   private:
      DataSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, DataBlob<bits>>(img, offset, env) {}
   };
   
   template <Bits bits>
   class LazySymbolPointerSection: public SectionT<bits, LazySymbolPointer<bits>> {
   public:
      template <typename... Args>
      static LazySymbolPointerSection<bits> *Parse(Args&&... args) {
         return new LazySymbolPointerSection<bits>(args...);
      }
   private:
      LazySymbolPointerSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, LazySymbolPointer<bits>>(img, offset, env) {}
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

      template <typename... Args>
      static LazySymbolPointer<bits> *Parse(Args&&... args) {
         return new LazySymbolPointer(args...);
      }
      
   private:
      LazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env);
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override {
         return pointee->loc.vmaddr;
      }
   };

   template <Bits bits>
   class NonLazySymbolPointer: public SymbolPointer<bits> {
   public:
      template <typename... Args>
      static NonLazySymbolPointer<bits> *Parse(Args&&... args) {
         return new NonLazySymbolPointer(args...);
      }
   private:
      NonLazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SymbolPointer<bits>(loc, env) {}
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override { return 0x0; }
   };

   template <Bits bits>
   class NonLazySymbolPointerSection: public SectionT<bits, NonLazySymbolPointer<bits>> {
   public:
      template <typename... Args>
      static NonLazySymbolPointerSection<bits> *Parse(Args&&... args) {
         return new NonLazySymbolPointerSection(args...);
      }
      
   private:
      NonLazySymbolPointerSection(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT<bits, NonLazySymbolPointer<bits>>(img, offset, env) {}
   };

   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t dstate;
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      unsigned memidx;
      const SectionBlob<bits> *memdisp; /*!< memory displacement pointee */
      const SectionBlob<bits> *brdisp;  /*!< branch displacement pointee */
      
      virtual std::size_t size() const override { return instbuf.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      template <typename... Args>
      static Instruction<bits> *Parse(Args&&... args) { return new Instruction(args...); }
      
   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env);
   };


};
