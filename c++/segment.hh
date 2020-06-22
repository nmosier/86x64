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

      virtual uint32_t cmd() const override { return segment_command.cmd; }
      virtual std::size_t size() const override;
      std::string name() const;
      std::size_t vmaddr() const { return segment_command.vmaddr; }

      Section<bits> *section(const std::string& name);

      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   protected:
      Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };   

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;

      std::string name() const;

      virtual ~Section() {}
      
      static std::size_t size() { return sizeof(section_t); }
      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);

   protected:
      Section(const Image& img, std::size_t offset): sect(img.at<section_t>(offset)) {}
   };

   template <Bits bits, class Elem>
   class SectionT: public Section<bits> {
   public:
      using Content = std::list<Elem *>;
      Content content;

      ~SectionT();
      
   protected:
      typedef Elem *(*Parser)(const Image&, const Location&, ParseEnv<bits>&);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser);
      SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         SectionT(img, offset, env, [](const Image& img, const Location& loc, ParseEnv<bits>& env) {
                                       return Elem::Parse(img, loc, env);
                                    }) {}
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
   };

   template <Bits bits>
   class SectionBlob {
   public:
      virtual std::size_t size() const = 0;
      virtual ~SectionBlob() {}

      static SectionBlob<bits> *Parse(const Image& img, std::size_t offset, std::size_t vmaddr,
                                      ParseEnv<bits>& env);
      
      
   protected:
      SectionBlob(const Location& loc, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      uint8_t data;
      virtual std::size_t size() const override { return 1; }
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
      
   protected:
      template <typename... Args>
      SymbolPointer(Args&&... args): SectionBlob<bits>(args...) {}
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
      const SectionBlob<bits> *memdisp; /*!< memory displacement pointee */
      const SectionBlob<bits> *brdisp;  /*!< branch displacement pointee */
      
      virtual std::size_t size() const override { return instbuf.size(); }

      template <typename... Args>
      static Instruction<bits> *Parse(Args&&... args) { return new Instruction(args...); }
      
   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env);
   };


};
