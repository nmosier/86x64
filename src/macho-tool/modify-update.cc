#include "modify-update.hh"
#include "util.hh"
#include "core/lc.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/dyldinfo.hh"
#include "core/symtab.hh"

Operation *ModifyCommand::Update::getop(int index) {
   switch (index) {
   case 0: // load_dylib
   case 1: // load-dylib
      return new LoadDylib;

   case 2: // bind
      return new BindNode;

   case 3: // strip-bind
      return new StripBind;

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

int ModifyCommand::Update::BindNode::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // old_sym
   case 1:
      old_sym = value;
      return 1;
      
   case 2: // new_type
   case 3:
      new_type = stout<uint8_t>(value, nullptr, 0);
      return 1;

   case 4: // new_dylib
   case 5:
      new_dylib_ord = stout<unsigned>(value, nullptr, 0);
      return 1;

   case 6: // new_sym
   case 7:
      new_sym = value;
      return 1;

   case 8: // new_flags
   case 9:
      new_flags = stout<uint8_t>(value, nullptr, 0);
      return 1;

   case 10: // lazy
      if (value) {
         throw std::string("`lazy' flag takes no arguments");
      }
      lazy = true;
      return 1;

   default: abort();
   }
}

void ModifyCommand::Update::BindNode::validate() const {
   if (!old_sym) {
      throw std::string("must specify original symbol name with `old_sym=<sym>'");
   }
}

void ModifyCommand::Update::BindNode::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
         if (lazy) {
            workT<MachO::Bits::M32, true>(archive);
         } else {
            workT<MachO::Bits::M32, false>(archive);
         }
         break;
      }
   case MachO::Bits::M64:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
         if (lazy) {
            workT<MachO::Bits::M64, true>(archive);
         } else {
            workT<MachO::Bits::M64, false>(archive);
         }
         break;
      }
   default: abort();
   }   
}

template <MachO::Bits b, bool l>
void ModifyCommand::Update::BindNode::workT(MachO::Archive<b> *archive) {
   auto load_dylibs = archive->template subcommands<MachO::DylibCommand, LC_LOAD_DYLIB>();

   /* find bind node */
   auto dyldinfo = archive->template subcommand<MachO::DyldInfo>();
   if (dyldinfo == nullptr) {
      throw MachO::error("LC_DYLDINFO command missing");
   }

   MachO::BindInfo<b, l> *bindinfo;
   if constexpr (l) {
         bindinfo = dyldinfo->lazy_bind;
} else {
      bindinfo = dyldinfo->bind;
   }

   auto bindee_it = bindinfo->find(*old_sym);
   if (bindee_it == bindinfo->end()) {
      throw MachO::error("bind node for symbol `%s' not found", old_sym->c_str());
   }
   auto bindee = *bindee_it;
   
   if (new_type) { bindee->type = *new_type; }
   if (new_addend) { bindee->addend = *new_addend; }
   if (new_dylib_ord) { bindee->dylib = load_dylibs[*new_dylib_ord - 1]; }
   if (new_sym) { bindee->sym = *new_sym; }
   if (new_flags) { bindee->flags = *new_flags; }
}

int ModifyCommand::Update::StripBind::subopthandler(int index, char *value) {
   switch (index) {
   case 0: // suffix
      suffixes.emplace_back(value);
      return 1;
   default: abort();
   }
}

void ModifyCommand::Update::StripBind::operator()(MachO::MachO *macho) {
   switch (macho->bits()) {
   case MachO::Bits::M32:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho);
         workT<MachO::Bits::M32>(archive);
         break;
      }
   case MachO::Bits::M64:
      {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
         workT<MachO::Bits::M64>(archive);
         break;
      }
   default: abort();
   }   
}

template <MachO::Bits b>
void ModifyCommand::Update::StripBind::workT(MachO::Archive<b> *archive) {
   auto dyld_info = archive->template subcommand<MachO::DyldInfo>();
   if (dyld_info == nullptr) {
      throw std::string("missing dyld info");
   }
   
   workT(dyld_info->bind);
   workT(dyld_info->lazy_bind);

#if 0
   auto symtab = archive->template subcommand<MachO::Symtab>();
   if (symtab == nullptr) {
      throw std::string("missing symbol table");
   }
   for (auto str : symtab->strs) {
      strip(str->str);
   }
#endif
}

template <MachO::Bits b, bool lazy>
void ModifyCommand::Update::StripBind::workT(MachO::BindInfo<b, lazy> *bind_info) {
   for (auto bindee : bind_info->bindees) {
      strip(bindee->sym);
   }
}


void ModifyCommand::Update::StripBind::strip(std::string& s) const {
   for (const std::string& suffix : suffixes) {
      const auto index = s.find(suffix);
      if (index != std::string::npos) {
         s.erase(index, suffix.size());
      }
   }
}
