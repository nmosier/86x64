#include <iostream>
#include <fcntl.h>

#include "print.hh"
#include "macho.hh"
#include "archive.hh"
#include "dyldinfo.hh"
#include "rebase_info.hh"

int PrintCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;

   case REBASE:
      ops.push_back(REBASE);
      return 1;
      
   default: abort();
   }
}

int PrintCommand::work() {
   const MachO::MachO *macho = MachO::MachO::Parse(*img);
   switch (macho->bits()) {
   case MachO::Bits::M32: return workT<MachO::Bits::M32>(macho);
   case MachO::Bits::M64: return workT<MachO::Bits::M64>(macho);
   }
}

template <MachO::Bits bits>
int PrintCommand::workT(const MachO::MachO *macho) {
   const auto archive = dynamic_cast<const MachO::Archive<bits> *>(macho);
   if (archive == nullptr) {
      throw std::string("printing requires archive");
   }
   
   for (int op : ops) {
      switch (op) {
      case REBASE:
         print_REBASE<bits>(archive);
         break;
      default: abort();
      }
   }
   return 0;
}

template <MachO::Bits bits>
void PrintCommand::print_REBASE(const MachO::Archive<bits> *archive) {
   const auto dyld_info = archive->template subcommand<MachO::DyldInfo>();
   if (dyld_info == nullptr) {
      throw std::string("archive does not contain LC_DYLD_INFO command");
   }
   dyld_info->rebase->print(std::cout << std::hex);
}

PrintCommand::PrintCommand(): InplaceCommand("print", O_RDONLY) {}
