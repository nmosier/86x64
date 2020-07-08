#include <iostream>
#include <libgen.h>
#include <mach/machine.h>

#include "convert.hh"
#include "util.hh"
#include "macho.hh"
#include "archive.hh"
#include "symtab.hh"

int ConvertCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
      
   case 'a':
      {
         auto it = mach_header_filetype_map.find(optarg);
         if (it != mach_header_filetype_map.end()) {
            filetype = it->second;
         } else {
            throw std::string("invalid Mach-O archive filetype");
         }
      }
      return 1;

   default: abort();
   }
}

int ConvertCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
      
   if (filetype) {
      auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (archive == nullptr) {
         log("MachO binary is not an archive");
         return -1;
      }
         
      /* change filetype */
      archive->header.filetype = *filetype;
         
      switch (*filetype) {
      case MH_DYLIB:
         archive_EXECUTE_to_DYLIB(archive);
         break;
            
      default:
         log("unsupported Mach-O archive filetype for conversion");
         return -1;
      }
   }

   macho->Build();
   macho->Emit(*out_img);
      
   return 0;
}

void ConvertCommand::archive_EXECUTE_to_DYLIB(MachO::Archive<MachO::Bits::M64> *archive) {
   /* add LC_ID_DYLIB */
   char *out_path = strdup(this->out_path);
   char *name = basename(out_path);
   auto id_dylib = MachO::DylibCommand<MachO::Bits::M64>::Create(LC_ID_DYLIB, name);
   archive->load_commands.push_back(id_dylib);
   free(out_path);

   /* check whether MH_NO_REEXPORTED_DYLIBS should be added */
   if (archive->template subcommands<MachO::LoadCommand, LC_REEXPORT_DYLIB>().empty()) {
      archive->header.flags |= MH_NO_REEXPORTED_DYLIBS;
   }

   /* remove PAGEZERO */
   for (auto it = archive->load_commands.begin(); it != archive->load_commands.end(); ++it) {
      auto segment = dynamic_cast<MachO::Segment<MachO::Bits::M64> *>(*it);
      if (segment && strcmp(SEG_PAGEZERO, segment->segment_command.segname) == 0) {
         it = archive->load_commands.erase(it);
      }
   }

   /* remove __mh_execute_header */
   auto symtab = archive->template subcommand<MachO::Symtab>();
   if (symtab == nullptr) {
      throw std::string("missing symtab");
   }
   symtab->remove("__mh_execute_header");

   /* remove irrelevant commands */
   archive->remove_commands(LC_LOAD_DYLINKER);
   archive->remove_commands(LC_MAIN);

   /* tweak archive header */
   archive->header.cpusubtype &= ~ (uint32_t) CPU_SUBTYPE_LIB64;
   archive->header.flags &= ~ (uint32_t) MH_PIE;

   /* change base address */
   archive->vmaddr = 0x0;
}
