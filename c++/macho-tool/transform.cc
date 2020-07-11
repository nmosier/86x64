#include <iostream>

#include "transform.hh"
#include "archive.hh"

int TransformCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h': // help
      usage(std::cout);
      return 0;
      
   case 'm': // bits
      switch (std::stoi(optarg)) {
      case 32:
         bits = MachO::Bits::M32;
         break;
      case 64:
         bits = MachO::Bits::M64;
      default:
         throw std::string("bits must be 32 or 64");
      }
      return 1;
      
   default:
      abort();
   }
}

template <MachO::Bits b>
int TransformCommand::workT(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<b> *>(macho);
   if (archive == nullptr) {
      log("transform requires archive of correct bits");
      return -1;
   }
   auto newarchive = archive->Transform();
   newarchive->Build(0);
   newarchive->Emit(*out_img);
   return 0;
}

int TransformCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);

   switch (bits ? *bits : macho->bits()) {
   case MachO::Bits::M32: return workT<MachO::Bits::M32>(macho);
   case MachO::Bits::M64: return workT<MachO::Bits::M64>(macho);
   }
}
