#include <iostream>

#include "rebasify.hh"
#include "macho.hh"
#include "archive.hh"

int Rebasify::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
      
   default: abort();
   }
}

int Rebasify::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   auto archive32 = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
   if (archive32 == nullptr) {
      log("input Mach-O not a 32-bit archive");
      return -1;
   }

   // TODO

   archive32->Build(0);
   archive32->Emit(*out_img);
   return 0;
}
