#include "modify-update.hh"
#include "util.hh"
#include "lc.hh"
#include "macho.hh"
#include "archive.hh"

Operation *ModifyCommand::Update::getop(int index) {
   switch (index) {
   case 0: // load_dylib
   case 1: // load-dylib
      return new LoadDylib;

   default: abort();
   }
}

int ModifyCommand::Update::LoadDylib::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // name
      name = value;
      return 1;
   case 1: // timestamp
      timestamp = stout<uint32_t>(value);
      return 1;
   case 2: // current_version
      current_version = stout<uint32_t>(value);
      return 1;
   case 3: // compatibility_version
      compatibility_version = stout<uint32_t>(value);
      return 1;
   case 4: // lc
      lc = stout<unsigned>(value);
      return 1;

   default: abort();
   }
}

void ModifyCommand::Update::LoadDylib::validate() const {
   if (!lc) {
      throw std::string("provide load commad id with `lc=<id>'");
   }
}

template <MachO::Bits b>
void ModifyCommand::Update::LoadDylib::workT(MachO::Archive<b> *archive) {
   auto load_dylib = dynamic_cast<MachO::DylibCommand<b> *>(archive->load_commands.at(*lc));
   if (load_dylib == nullptr) {
      throw std::string("load command %d is not a LC_LOAD_DYLIB command", *lc);
   }

   if (name) { load_dylib->name = *name; }
   if (timestamp) { load_dylib->dylib_cmd.dylib.timestamp = *timestamp; }
   if (current_version) { load_dylib->dylib_cmd.dylib.current_version = *current_version; }
   if (compatibility_version) {
      load_dylib->dylib_cmd.dylib.compatibility_version = *compatibility_version;
   }
              
}

void ModifyCommand::Update::LoadDylib::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      workT<MachO::Bits::M32>(dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho));
      break;
   case MachO::Bits::M64:
      workT<MachO::Bits::M64>(dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho));
      break;
   default: abort();
   }
}
