#include "modify-start.hh"
#include "core/archive.hh"

int ModifyCommand::Start::parse(char *option) {
   if (option == nullptr) {
      throw std::string("missing argument");
   }
   
   vmaddr = std::stoll(option, 0, 0);

   return 1;
}

void ModifyCommand::Start::operator()(MachO::MachO *macho) {
   auto archive32 = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
   auto archive64 = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive32) {
      archive32->vmaddr = this->vmaddr;
   } else if (archive64) {
      archive64->vmaddr = this->vmaddr;
   } else {
      throw std::string("binary is not an archive");
   }
}
