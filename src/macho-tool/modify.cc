#include "modify.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/instruction.hh"
#include "modify-insert.hh"
#include "modify-delete.hh"
#include "modify-start.hh"
#include "modify-update.hh"

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
   case 'u':
      f = new Update;
      break;
   default:
      abort();
   }

   operations.emplace_back(f);
   return f->parse(optarg);
}
