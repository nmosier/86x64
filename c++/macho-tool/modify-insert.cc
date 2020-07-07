#include "modify-insert.hh"
#include "archive.hh"
#include "instruction.hh"


void ModifyCommand::Insert::Instruction::operator()(MachO::MachO *macho) {
   MachO::Archive<MachO::Bits::M64> *archive =
      dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (!archive) {
      throw std::logic_error("only 64-bit archives currently supported");
   }
   
   /* read bytes */
   char *buf = new char[*bytes];
   assert(fread(buf, 1, *bytes, stdin) == *bytes);
   
   archive->insert(new MachO::Instruction<MachO::Bits::M64>(archive->segment(SEG_TEXT),
                                                            buf, buf + *bytes), loc, relation);
}




int ModifyCommand::Insert::Instruction::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // vmaddr
      loc.vmaddr = std::stoul(value, nullptr, 0);
      break;
   case 1: // offset
      loc.offset = std::stoul(value, nullptr, 0);
      break;
   case 2: // before
      relation = MachO::Relation::BEFORE;
      break;
   case 3: // after
      relation = MachO::Relation::AFTER;
      break;
   case 4: // bytes
      bytes = std::stoul(value, nullptr, 0);
      break;
   default: abort();
   }
   return 1;
}


void ModifyCommand::Insert::Instruction::validate() const {
   if (!bytes) {
      throw std::string("specify instruction size with `bytes'");
   }
}

Operation *ModifyCommand::Insert::getop(int index) {
   switch (index) {
   case 0:
   case 1:
   case 2:
      return new Instruction;
      
   default: abort();
   }
}
