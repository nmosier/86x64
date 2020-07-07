#include "modify-delete.hh"
#include "archive.hh"
#include "instruction.hh"
#include "symtab.hh"

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

Functor *ModifyCommand::Delete::getop(int index) {
   switch (index) {
   case 0:
   case 1:
   case 2:
      return new Instruction;

   case 3:
   case 4:
   case 5:
      return new Segment;

   case 6:
   case 7:
      return new Symbol;
      
   default: abort();
   }
}

int ModifyCommand::Delete::Segment::parse(char *option) {
   if (*option == '\0') {
      throw std::string("segment: requires segment name");
   }
   name = option;
   return 1;
}

void ModifyCommand::Delete::Segment::operator()(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive == nullptr) {
      throw std::string("deleting segment requires 64-bit archive");
   }
   
   for (auto it = archive->load_commands.begin(); it != archive->load_commands.end(); ++it) {
      auto segment = dynamic_cast<MachO::Segment<MachO::Bits::M64> *>(*it);
      if (segment != nullptr && name == segment->segment_command.segname) {
         archive->load_commands.erase(it);
         return;
      }
   }

   std::cerr << "warning: segment `" << name << "' not found in original" << std::endl;
}

int ModifyCommand::Delete::Symbol::parse(char *option) {
   if (*option == '\0') {
      throw std::string("missing symbol name");
   } else {
      name = option;
   }
   return 1;
}

void ModifyCommand::Delete::Symbol::operator()(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   auto symtab = archive->template subcommand<MachO::Symtab>();
   if (symtab == nullptr) {
      throw std::string("missing symbol table");
   }
   
   symtab->remove(name);
}
