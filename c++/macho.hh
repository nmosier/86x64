#pragma once

#include <memory>
#include <list>
#include <vector>

#include <stdint.h>
#include <string.h>
#include <fcntl.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "macho-fwd.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits> using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits> using macho_size_t = macho_addr_t<bits>;
   template <Bits bits> using mach_header_t = select_type<bits, mach_header, mach_header_64>;

   class Image {
   public:
      Image(const char *path);
      ~Image();

      std::size_t size() const { return filesize; }

      template <typename T>
      const T& at(std::size_t index) const { return * (const T *) ((const char *) img + index); }

      template <typename T>
      T& at(std::size_t index) { return * (T *) ((char *) img + index); }

      Image(const Image&) = delete;

   private:
      int fd;
      std::size_t filesize;
      void *img;

      void resize(std::size_t newsize);
   };
   


   class MachO {
   public:
      virtual uint32_t magic() const = 0;
      virtual uint32_t& magic() = 0;
      
      static MachO *Parse(const Image& img);
      virtual ~MachO() {}

   private:
   };

   template <Bits bits>
   class Archive: public MachO {
   public:
      using mach_header_t = select_type<bits, mach_header, mach_header_64>;
      using LoadCommands = std::vector<LoadCommand<bits> *>;

      mach_header_t header;
      LoadCommands load_commands;

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }

      static Archive<bits> *Parse(const Image& img, std::size_t offset = 0);
      virtual ~Archive() override;

   private:
      Archive() {}
      Archive(const mach_header_t& header, const LoadCommands& load_commands):
         header(header), load_commands(load_commands) {}
   };

   template <Bits bits>
   class LoadCommand {
   public:
      virtual std::size_t size() const = 0;

      static LoadCommand<bits> *Parse(const Image& img, std::size_t offset);
      virtual ~LoadCommand() {}
      
   protected:
      LoadCommand() {}
   };

   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using segment_command_t = select_type<bits, segment_command, segment_command_64>;
      using Sections = std::vector<Section<bits> *>;

      segment_command_t segment_command;
      Sections sections;
      
      virtual std::size_t size() const override;

      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   protected:
      Segment(const Image& img, std::size_t offset);
   };

#if 0
   template <Bits bits>
   class TEXTSegment: public Segment<bits> {
   public:

      template <typename... Args>
      static TEXTSegment<bits> *Parse(Args&&... args) { return new TEXTSegment(args...); }
      
   private:
      TEXTSegment(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class BSSSegment: public Segment<bits> {
   public:
      template <typename... Args>
      static BSSSegment<bits> *Parse(Args&&... args) { return new BSSSegment(args...); }
   private:
      BSSSegment(const Image& img, std::size_t offset);
   };
#endif

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;
      std::vector<char> data;

      static std::size_t size() { return sizeof(section_t); }
      
      static Section<bits> *Parse(const Image& img, std::size_t offset);
      
   private:
      Section() {}
   };

   template <Bits bits>
   class Section_ {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;

      static std::size_t size() { return sizeof(section_t); }

      static Section_<bits> *Parse(const Image& img, std::size_t offset);
      
   protected:
      Section_(const Image& img, std::size_t offset): sect(img.at<section_t>(offset)) {}
   };

   template <Bits bits>
   class TextSection: public Section_<bits> {
   public:
      using Instructions = std::vector<Instruction<bits> *>;

      Instructions instructions;

      template <typename... Args>
      static TextSection<bits> *Parse(Args&&... args) { return new TextSection(args...); }
      
   private:
      TextSection(const Image& img, std::size_t offset);
   }; // TODO: need to disassemble into instructions. But also functions, right? Nah, not yet. Patience.

   template <Bits bits>
   class DataSection: public Section_<bits> {
   public:
      std::vector<uint8_t> data;

      template <typename... Args>
      static DataSection<bits> *Parse(Args&&... args) { return new DataSection(args...); }
      
   private:
      DataSection(const Image& img, std::size_t offset):
         Section_<bits>(img, offset), data(&img.at<uint8_t>(this->sect.offset),
                                           &img.at<uint8_t>(this->sect.offset + this->sect.size))
      {}
   };

   template <Bits bits>
   class SymbolPointerSection: public Section_<bits> {
   public:
      using pointer_t = select_type<bits, uint32_t, uint64_t>;

      std::vector<pointer_t> pointers;

      template <typename... Args>
      static SymbolPointerSection<bits> *Parse(Args&&... args)
      { return new SymbolPointerSection(args...); }
      
   private:
      SymbolPointerSection(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class DyldInfo: public LoadCommand<bits> {
   public:
      dyld_info_command dyld_info;
      std::vector<uint8_t> rebase;
      std::vector<uint8_t> bind;
      std::vector<uint8_t> weak_bind;
      std::vector<uint8_t> lazy_bind;
      std::vector<uint8_t> export_info;

      virtual std::size_t size() const { return sizeof(dyld_info); }

      static DyldInfo<bits> *Parse(const Image& img, std::size_t offset);
      
   private:
      DyldInfo() {}
   };

   template <Bits bits>
   class Symtab: public LoadCommand<bits> {
   public:
      using Nlists = std::vector<Nlist<bits> *>;
      using Strings = std::vector<std::string *>;
      
      symtab_command symtab;
      Nlists syms;
      Strings strs;
      

      virtual std::size_t size() const { return sizeof(symtab); }

      template <typename... Args>
      static Symtab<bits> *Parse(Args&&... args) { return new Symtab(args...); }
      
   private:
      Symtab(const Image& img, std::size_t offset);
      
   };

   template <Bits bits>
   class Nlist {
   public:
      using nlist_t = select_type<bits, struct nlist, struct nlist_64>;

      static std::size_t size() { return sizeof(nlist_t); }

      nlist_t nlist;
      std::string *string;
      
      template <typename... Args>
      static Nlist<bits> *Parse(Args&&... args) { return new Nlist(args...); }
   private:
      Nlist(const Image& img, std::size_t offset,
            const std::unordered_map<std::size_t, std::string *>& off2str);
   };

   template <Bits bits>
   class Dysymtab: public LoadCommand<bits> {
   public:
      dysymtab_command dysymtab;
      std::vector<uint32_t> indirectsyms;
      
      virtual std::size_t size() const override { return sizeof(dysymtab_command); }

      template <typename... Args>
      static Dysymtab<bits> *Parse(Args&&... args) { return new Dysymtab(args...); }
      
   private:
      Dysymtab(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class DylinkerCommand: public LoadCommand<bits> {
   public:
      dylinker_command dylinker;
      std::string name;

      virtual std::size_t size() const override;

      template <typename... Args>
      static DylinkerCommand<bits> *Parse(Args&&... args) { return new DylinkerCommand(args...); }
      
   private:
      DylinkerCommand(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class UUID: public LoadCommand<bits> {
   public:
      uuid_command uuid;

      virtual std::size_t size() const override { return sizeof(uuid_command); }

      template <typename... Args>
      static UUID<bits> *Parse(Args&&... args) { return new UUID(args...); }
      
   private:
      UUID(const Image& img, std::size_t offset): uuid(img.at<uuid_command>(offset)) {}
   };

   template <Bits bits>
   class BuildVersion: public LoadCommand<bits> {
   public:
      build_version_command build_version;
      std::vector<BuildToolVersion<bits> *> tools;
      
      virtual std::size_t size() const override;

      template <typename... Args>
      static BuildVersion<bits> *Parse(Args&&... args) { return new BuildVersion(args...); }
      
   private:
      BuildVersion(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class BuildToolVersion {
   public:
      build_tool_version tool;

      static std::size_t size() { return sizeof(build_tool_version); }
      
      template <typename... Args>
      static BuildToolVersion<bits> *Parse(Args&&... args) { return new BuildToolVersion(args...); }
      
   private:
      BuildToolVersion(const Image& img, std::size_t offset):
         tool(img.at<build_tool_version>(offset)) {}
   };

   template <Bits bits>
   class SourceVersion: public LoadCommand<bits> {
   public:
      source_version_command source_version;

      virtual std::size_t size() const override { return sizeof(source_version); }
      
      template <typename... Args>
      static SourceVersion<bits> *Parse(Args&&... args) {
         return new SourceVersion(args...);
      }
      
   private:
      SourceVersion(const Image& img, std::size_t offset):
         source_version(img.at<source_version_command>(offset)) {}
   };

   template <Bits bits>
   class EntryPoint: public LoadCommand<bits> {
   public:
      entry_point_command entry_point;
      // TODO: needs to point to entry point instruction
      
      virtual std::size_t size() const override { return sizeof(entry_point); }
      
      template <typename... Args>
      static EntryPoint<bits> *Parse(Args&&... args) { return new EntryPoint(args...); }
      
   private:
      EntryPoint(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class DylibCommand: public LoadCommand<bits> {
   public:
      dylib_command dylib_cmd;
      std::string name;

      virtual std::size_t size() const override;

      template <typename... Args>
      static DylibCommand<bits> *Parse(Args&&... args) { return new DylibCommand(args...); }
      
   private:
      DylibCommand(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class LinkeditData: public LoadCommand<bits> {
   public:
      linkedit_data_command linkedit;
      std::vector<uint8_t> function_starts; // TODO: parse completely

      virtual std::size_t size() const override { return sizeof(linkedit); }
      
      template <typename... Args>
      static LinkeditData<bits> *Parse(Args&&... args) { return new LinkeditData(args...); }
      
   private:
      LinkeditData(const Image& img, std::size_t offset);
   };

   template <Bits bits>
   class Instruction {
   public:
      static const xed_state_t dstate;
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      
      std::size_t size() const { return instbuf.size(); }
      
      template <typename... Args>
      static Instruction<bits> *Parse(Args&&... args) { return new Instruction(args...); }
      
   private:
      Instruction(const Image& img, std::size_t offset);
   };

}

