#include "modify-delete.hh"
#include "archive.hh"
#include "instruction.hh"

int ModifyCommand::Delete::Instruction::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // vmaddr
      kind = LocationKind::VMADDR;
      loc = std::stoul(value, nullptr, 0);
      return 1;
   case 1: // offset
      kind = LocationKind::OFFSET;
      loc = std::stoul(value, nullptr, 0);
      return 1;
   default: abort();
   }
}

void ModifyCommand::Delete::Instruction::operator()(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive) {
      if (kind == LocationKind::OFFSET) {
         loc = archive->offset_to_vmaddr(loc);
      }
      auto blob = archive->find_blob<MachO::Instruction>(loc);
      blob->active = false;
   } else {
      std::stringstream ss;
      ss << "no blob found at vmaddr " << std::hex << loc;
      throw ss.str();
   }
}

Operation *ModifyCommand::Delete::getop(int index) {
   switch (index) {
   case 0:
   case 1:
   case 2:
      return new Instruction;
      
   default: abort();
   }
}

