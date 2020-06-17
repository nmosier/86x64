#pragma once

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


}

