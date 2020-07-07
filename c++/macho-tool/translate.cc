#include <string>
#include <iostream>
#include <fcntl.h>

#include "translate.hh"
#include "macho.hh"
#include "archive.hh"

int TranslateCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
   case 'o':
      {
         char *endptr;
         offset = std::strtoul(optarg, &endptr, 0);
         if (*optarg == '\0' || *endptr) {
            throw std::string("invalid offset");
         }
      }
      return 1;
         
   default: abort();
   }
}

int TranslateCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*img);
   
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive == nullptr) {
      log("only 64-bit archives supported");
      return -1;
   }
   
   if (offset) {
      std::cout << std::hex << "0x" << archive->offset_to_vmaddr(offset) << std::endl;
   }
   
   return 0;
}

TranslateCommand::TranslateCommand(): InplaceCommand("translate", O_RDONLY) {}
