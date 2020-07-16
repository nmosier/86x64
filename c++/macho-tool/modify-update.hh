#pragma once

#include <optional>

#include "modify.hh"

struct ModifyCommand::Update: Subcommand {
   struct LoadDylib;
   struct BindNode;

   virtual std::vector<char *> keylist() const override {
      return {"load_dylib", "load-dylib",
              "bind",
              nullptr};
   }

   virtual Operation *getop(int index) override;
};

struct ModifyCommand::Update::LoadDylib: Operation {
   std::optional<unsigned> lc;
   std::optional<std::string> name;
   std::optional<uint32_t> timestamp;
   std::optional<uint32_t> current_version;
   std::optional<uint32_t> compatibility_version;

   virtual std::vector<char *> keylist() const override {
      return {"name", "timestamp", "current_version", "compatibility_version", "lc", nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override;
   virtual void operator()(MachO::MachO *macho) override;
   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
};

struct ModifyCommand::Update::BindNode: Operation {
   std::optional<std::string> old_sym;
   bool lazy = false;
   
   std::optional<uint8_t> new_type;
   std::optional<ssize_t> new_addend;
   std::optional<unsigned> new_dylib_ord;
   std::optional<std::string> new_sym;
   std::optional<uint8_t> new_flags;
   // std::optional<std::size_t> new_vmaddr;

   virtual std::vector<char *> keylist() const override {
      return {"old_sym", "old-sym",
              "new_type", "new-type",
              "new_dylib", "new-dylib",
              "new_sym", "new-sym",
              "new_flags", "new-flags",
              "lazy",
              nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override;
   virtual void operator()(MachO::MachO *macho) override;   
   template <MachO::Bits b, bool lazy> void workT(MachO::Archive<b> *archive);
};
