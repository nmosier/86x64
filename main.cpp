#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <numeric>

#include <LIEF/LIEF.hpp>
extern "C" {
#include <xed/xed-interface.h>
}

#include "instrs.hpp"
#include "function.hpp"
#include "visitor.hpp"

void usage(FILE *f, int argc, char *argv[]) {
   fprintf(f,
           "usage: %1$s <macho>\n"              \
           "       %1$s -h\n",
           argv[0]
           );
}

int main(int argc, char *argv[]) {
   const char *optstring = "h";
   int optchar;
   while ((optchar = getopt(argc, argv, optstring)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout, argc, argv);
         return 0;
      case '?':
         usage(stderr, argc, argv);
         return 1;
      }
   }

   if (argc - optind != 1) {
      usage(stderr, argc, argv);
      return 1;
   }

   const char *in_path = argv[optind++];

   // XED initialization
   xed_tables_init();
 
   std::unique_ptr<LIEF::MachO::FatBinary> macho = LIEF::MachO::Parser::parse(in_path);



   for (auto& binary : *macho) {
      // update header
      auto& hdr = binary.header();
      hdr.magic(hdr.magic() == LIEF::MachO::MACHO_TYPES::MH_MAGIC ? LIEF::MachO::MACHO_TYPES::MH_MAGIC_64 : LIEF::MachO::MACHO_TYPES::MH_CIGAM_64);
      hdr.cpu_type(LIEF::MachO::CPU_TYPES::CPU_TYPE_X86_64);
      hdr.cpu_subtype((uint32_t) LIEF::MachO::CPU_SUBTYPES_X86::CPU_SUBTYPE_X86_64_ALL);
      // NOTE: update sizeofcmds later

      uint64_t cmd_offset_diff = sizeof(LIEF::MachO::mach_header_64) -
         sizeof(LIEF::MachO::mach_header);
      for (LIEF::MachO::SegmentCommand& cmd : binary.segments()) {
         uint64_t cmd_size_diff = sizeof(LIEF::MachO::segment_command_64) -
            sizeof(LIEF::MachO::segment_command_32);
         cmd.command(LIEF::MachO::LOAD_COMMAND_TYPES::LC_SEGMENT_64);
         cmd.size(cmd.size() + cmd_size_diff);
         cmd.command_offset(cmd.command_offset() + cmd_offset_diff);
         cmd_offset_diff += cmd_size_diff;

         
      }

      // update sizeof cmds
      binary
         .header()
         .sizeof_cmds(std::accumulate(binary.commands().begin(), binary.commands().end(), 0,
                                      [](auto acc, auto rhs){
                                         return acc + rhs.size();
                                      })
                      );
   }

   
   macho->write("a.out");
   
   return 0;
}
