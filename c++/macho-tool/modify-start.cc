#include "modify-start.hh"
#include "archive.hh"

int ModifyCommand::Start::parse(char *option) {
   if (option == nullptr) {
      throw std::string("missing argument");
   }
   
   vmaddr = std::stoll(option, 0, 0);

   return 1;
}

void ModifyCommand::Start::operator()(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive) {
      archive->vmaddr = this->vmaddr;
   } else {
      throw std::string("binary is not an archive");
   }
}
