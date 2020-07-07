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
   Functor *f;
   
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;         
   case 'i':
      f = new Insert;
      break;
   case 'd':
      f = new Delete;
      break;
   case 's':
      f = new Start;
      break;
   default:
      abort();
   }

   operations.emplace_back(f);
   return f->parse(optarg);
}
