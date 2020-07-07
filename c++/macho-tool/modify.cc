#include "modify.hh"
#include "macho.hh"
#include "archive.hh"
#include "instruction.hh"
#include "modify-insert.hh"
#include "modify-delete.hh"
#include "modify-start.hh"

int ModifyCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   
   for (auto& op : operations) {
      (*op)(macho);
   }
   
   macho->Build();
   macho->Emit(*out_img);
   return 0;
}

int ModifyCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;         
   case 'i':
      operations.push_back(std::make_unique<Insert>());
      return 1;
   case 'd':
      operations.push_back(std::make_unique<Delete>());
      return 1;
   case 's':
      operations.push_back(std::make_unique<Start>());
      return 1;
   default:
      abort();
   }
}
