#pragma once

#include <optional>

#include "modify.hh"

struct ModifyCommand::Insert: Subcommand {
   struct Instruction;
   struct LoadDylib;
   
   virtual std::vector<char *> keylist() const override {
      return {"i", "inst", "instruction",
              "load_dylib", "load-dylib",
              nullptr};
   }

   virtual Operation *getop(int index) override;
};

struct ModifyCommand::Insert::Instruction: Operation {
   MachO::Location loc;
   MachO::Relation relation = MachO::Relation::BEFORE;
   std::optional<unsigned long> bytes;

   virtual std::vector<char *> keylist() const override {
      return {"vmaddr", "offset", "before", "after", "bytes", nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void operator()(MachO::MachO *macho) override;
   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
   virtual void validate() const override;
};

struct ModifyCommand::Insert::LoadDylib: Operation {
   std::optional<std::string> name;
   uint32_t timestamp = 0;
   uint32_t current_version = 0;
   uint32_t compatibility_version = 0;

   virtual std::vector<char *> keylist() const override {
      return {"name", "timestamp", "current_version", "compatibility_version", nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void operator()(MachO::MachO *macho) override;
   virtual void validate() const override;

   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
};
