#include "noop.hh"
#include "macho.hh"
#include "image.hh"


int NoopCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
   default: abort();
   }
}

int NoopCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   macho->Build();
   macho->Emit(*out_img);
   return 0;
}
