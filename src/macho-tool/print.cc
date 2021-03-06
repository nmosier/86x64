#include <iostream>
#include <fcntl.h>

#include "print.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/dyldinfo.hh"
#include "core/rebase_info.hh"
#include "core/symtab.hh"

int PrintCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;

   case REBASE:
      ops.push_back(REBASE);
      return 1;

   case SYMS:
      ops.push_back(SYMS);
      return 1;

   case LAZY_BIND:
      ops.push_back(LAZY_BIND);
      return 1;

   case BIND:
      ops.push_back(BIND);
      return 1;
      
   default: abort();
   }
}

int PrintCommand::work() {
   const MachO::MachO *macho = MachO::MachO::Parse(*img);
   std::cout << std::hex;
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
      case SYMS:
         print_SYMS<bits>(archive);
         break;
      case LAZY_BIND:
         print_LAZY_BIND<bits>(archive);
         break;
      case BIND:
         print_BIND<bits>(archive);
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
   dyld_info->rebase->print(std::cout);
}

template <MachO::Bits bits>
void PrintCommand::print_SYMS(const MachO::Archive<bits> *archive) {
   const auto symtab = archive->template subcommand<MachO::Symtab>();
   if (symtab == nullptr) {
      throw std::string("archive contains no symbol table");
   }

   symtab->print(std::cout);
}

template <MachO::Bits bits>
void PrintCommand::print_LAZY_BIND(const MachO::Archive<bits> *archive) {
   const auto dyld_info = archive->template subcommand<MachO::DyldInfo>();
   if (dyld_info == nullptr) {
      throw std::string("archive contains no dyld info");
   }

   dyld_info->lazy_bind->print(std::cout);
}

template <MachO::Bits bits>
void PrintCommand::print_BIND(const MachO::Archive<bits> *archive) {
   const auto dyld_info = archive->template subcommand<MachO::DyldInfo>();
   if (dyld_info == nullptr) {
      throw std::string("archive contains no dyld info");
   }
   dyld_info->bind->print(std::cout);
}

PrintCommand::PrintCommand(): InplaceCommand("print", O_RDONLY) {}
