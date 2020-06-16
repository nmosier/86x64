#pragma once

#include <list>
#include <vector>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "macho-fwd.hh"
#include "util.hh"
#include "image.hh"

namespace MachO {

   template <Bits bits> using macho_addr_t = select_type<bits, uint32_t, uint64_t>;
   template <Bits bits> using macho_size_t = macho_addr_t<bits>;
   template <Bits bits> using mach_header_t = select_type<bits, mach_header, mach_header_64>;   

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
      virtual uint32_t cmd() const = 0;
      virtual std::size_t size() const = 0;

      static LoadCommand<bits> *Parse(const Image& img, std::size_t offset);
      virtual ~LoadCommand() {}
      
   protected:
      LoadCommand() {}

      virtual void Parse2(const Image& img, Archive<bits>&& archive) {}
   };

   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using segment_command_t = select_type<bits, segment_command, segment_command_64>;
      using Sections = std::vector<Section<bits> *>;

      segment_command_t segment_command;
      Sections sections;

      virtual uint32_t cmd() const override { return segment_command.cmd; }
      virtual std::size_t size() const override;

      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   protected:
      Segment(const Image& img, std::size_t offset);
      virtual void Parse2(const Image& img, Archive<bits>&& archive) override;
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

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }

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
      
      virtual uint32_t cmd() const override { return symtab.cmd; }
      virtual std::size_t size() const override { return sizeof(symtab); }

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

      virtual uint32_t cmd() const override { return dysymtab.cmd; }      
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

      virtual uint32_t cmd() const override { return dylinker.cmd; }      
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

      virtual uint32_t cmd() const override { return uuid.cmd; }
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

      virtual uint32_t cmd() const override { return build_version.cmd; }      
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

      virtual uint32_t cmd() const override { return source_version.cmd; }      
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

      virtual uint32_t cmd() const override { return entry_point.cmd; }            
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

      virtual uint32_t cmd() const override { return dylib_cmd.cmd; }            
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

#if 0
   template <Bits bits>
   class DataInCodeEntry {
   public:
      data_in_code_entry dice;

      static std::size_t size() { return sizeof(dice); }

      template <typename... Args>
      static DataInCodeEntry<bits> *Parse(Args&&... args) { return new DataInCodeEntry(args...); }
      
   private:
      DataInCodeEntry(const Image& img, std::size_t offset):
         dice(img.at<data_in_code_entry>(offset)) {}
   };
#endif

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

}

