#include "modify.hh"
#include "macho.hh"
#include "archive.hh"
#include "instruction.hh"

int ModifyCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   
   for (auto& op : operations) {
      (*op)(macho);
   }
   
   macho->Build();
   macho->Emit(*out_img);
   return 0;
}

void ModifyCommand::Insert::operator()(MachO::MachO *macho) {
   MachO::Archive<MachO::Bits::M64> *archive =
      dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (!archive) {
      throw std::logic_error("only 64-bit archives currently supported");
   }
   
   /* read bytes */
   char *buf = new char[bytes];
   assert(fread(buf, 1, bytes, stdin) == bytes);
   
   archive->insert(new MachO::Instruction<MachO::Bits::M64>(archive->segment(SEG_TEXT),
                                                            buf, buf + bytes), loc, relation);
}

ModifyCommand::Insert::Insert(char *option) {
   char * const keylist[] = {"vmaddr", "offset", "before", "after", "bytes", nullptr};
   char *value;
   while (*optarg) {
      switch (getsubopt(&optarg, keylist, &value)) {
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
         bytes = std::stoi(value, nullptr, 0);
         break;
      case -1:
         if (suboptarg) {
            throw std::string("invalid suboption '") + suboptarg + "'";
         } else {
            throw std::string("missing suboption");
         }
      }
   }
         
   if (bytes == 0) {
      throw std::string("specify byte length with 'bytes'");
   }
}

ModifyCommand::Delete::Delete(char *option) {
   char * const keylist[] = {"vmaddr", "offset", nullptr};
   char *value;

   while (*optarg) {
      switch (getsubopt(&optarg, keylist, &value)) {
      case 0: // vmaddr
         kind = LocationKind::VMADDR;
         loc = std::stoul(value, nullptr, 0);
         break;
      case 1: // offset
         kind = LocationKind::OFFSET;
         loc = std::stoul(value, nullptr, 0);
         break;
      case -1:
         if (suboptarg) {
            throw std::string("invalid suboption `") + suboptarg + "'";
         } else {
            throw std::string("missing suboption `vmaddr' or `offset'");
         }
      }
   }
}

void ModifyCommand::Delete::operator()(MachO::MachO *macho) {
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

ModifyCommand::Start::Start(char *option) {
   if (option == nullptr) {
      throw std::string("missing argument");
   }
   
   vmaddr = std::stoll(option, 0, 0);
}

void ModifyCommand::Start::operator()(MachO::MachO *macho) {
   auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
   if (archive) {
      archive->vmaddr = this->vmaddr;
   } else {
      throw std::string("binary is not an archive");
   }
}

int ModifyCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;         
   case 'i':
      operations.push_back(std::make_unique<Insert>(optarg));
      return 1;
   case 'd':
      operations.push_back(std::make_unique<Delete>(optarg));
      return 1;
   case 's':
      operations.push_back(std::make_unique<Start>(optarg));
      return 1;
   default:
      abort();
   }
}
