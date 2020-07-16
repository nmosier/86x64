#include <string>
#include <iostream>
#include <fcntl.h>

#include "translate.hh"
#include "macho.hh"
#include "archive.hh"
#include "util.hh"

int TranslateCommand::opthandler(int optchar) {
   Functor *f;
   
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
   case 'o':
      f = new Offset;
      break;
   case LOAD_DYLIB:
      f = new LoadDylib;
      break;
      
   default: abort();
   }

   int parsestat = f->parse(optarg);
   op = std::unique_ptr<Functor>(f);
   return parsestat;
}

int TranslateCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*img);
   (*op)(macho);
   return 0;
}

TranslateCommand::TranslateCommand(): InplaceCommand("translate", O_RDONLY) {}

int TranslateCommand::Offset::parse(char *option) {
   offset = stout<std::size_t>(option, nullptr, 0);
   return 1;
}

void TranslateCommand::Offset::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);;
         workT(archive);
         break;
      }
   case MachO::Bits::M64:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);;
         workT(archive);
         break;
      }
   default: abort();
   }
}

template <MachO::Bits b>
void TranslateCommand::Offset::workT(MachO::Archive<b> *archive) {
   std::cout << std::hex << "0x" << archive->offset_to_vmaddr(*offset) << std::endl;   
}


int TranslateCommand::LoadDylib::parse(char *option) {
   name = option;
   return 1;
}

void TranslateCommand::LoadDylib::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);;
         workT(archive);
         break;
      }
   case MachO::Bits::M64:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);;
         workT(archive);
         break;
      }
   default: abort();
   }
}

template <MachO::Bits b>
void TranslateCommand::LoadDylib::workT(MachO::Archive<b> *archive) {
   auto load_dylibs = archive->template subcommands<MachO::DylibCommand, LC_LOAD_DYLIB>();
   auto it = std::find_if(load_dylibs.begin(), load_dylibs.end(),
                          [&] (auto load_dylib) {
                             return load_dylib->name == *name;
                          });
   if (it == load_dylibs.end()) {
      throw std::string("no LC_LOAD_DYLIB command with name `") + *name + "'";
   } else {
      std::cout << std::dec << it - load_dylibs.begin() + 1 << std::endl;
   }
}
