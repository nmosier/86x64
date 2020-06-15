#pragma once

#include <memory>
#include <list>
#include <vector>

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <mach-o/loader.h>

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
      
      virtual std::size_t size() const override { return segment_command.cmdsize; } // TODO

      static Segment<bits> *Parse(const Image& img, std::size_t offset);

      virtual ~Segment() override;
      
   private:
      Segment() {}
   };

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;
      std::vector<char> data;

      std::size_t size() const { return sizeof(section_t); }
      
      static Section<bits> *Parse(const Image& img, std::size_t offset);
      
   private:
      Section() {}
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
   
   
}

