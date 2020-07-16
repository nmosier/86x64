#include "modify-insert.hh"
#include "archive.hh"
#include "instruction.hh"
#include "util.hh"

void ModifyCommand::Insert::Instruction::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      return workT(dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho));
   case MachO::Bits::M64:
      return workT(dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho));
   default: abort();
   }
}

template <MachO::Bits b>
void ModifyCommand::Insert::Instruction::workT(MachO::Archive<b> *archive) {
   /* read bytes */
   char *buf = new char[*bytes];
   assert(fread(buf, 1, *bytes, stdin) == *bytes);
   archive->insert(new MachO::Instruction<b>(buf, buf + *bytes), loc, relation);
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

   case 3:
   case 4:
      return new LoadDylib;
      
   default: abort();
   }
}


int ModifyCommand::Insert::LoadDylib::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // name
      name = value;
      break;
   case 1: // timestamp
      timestamp = stout<uint32_t>(value, nullptr, 0);
      break;
   case 2: // current_version
      current_version = stout<uint32_t>(value, nullptr, 0);
      break;
   case 3: // compatibility_version
      compatibility_version = stout<uint32_t>(value, nullptr, 0);
      break;
   default: abort();
   }
   return 1;
}

void ModifyCommand::Insert::LoadDylib::validate() const {
   if (!name) {
      throw std::string("specify dylib name with `name'");
   }
}

void ModifyCommand::Insert::LoadDylib::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      return workT(dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho));
   case MachO::Bits::M64:
      return workT(dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho));
   default: abort();
   }
}

template <MachO::Bits b>
void ModifyCommand::Insert::LoadDylib::workT(MachO::Archive<b> *archive) {
   auto load_dylib = MachO::DylibCommand<b>::Create(LC_LOAD_DYLIB, *name, timestamp,
                                                    current_version, compatibility_version);
   archive->load_commands.push_back(load_dylib);
}
