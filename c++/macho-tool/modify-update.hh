#pragma once

#include <optional>

#include "modify.hh"

struct ModifyCommand::Update: Subcommand {
   struct LoadDylib;

   virtual std::vector<char *> keylist() const override {
      return {"load_dylib", "load-dylib",
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
