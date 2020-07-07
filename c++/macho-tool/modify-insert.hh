#pragma once

#include <optional>

#include "modify.hh"

struct ModifyCommand::Insert: Subcommand {
   struct Instruction;
   
   virtual std::vector<char *> keylist() const override {
      return {"i", "inst", "instruction", nullptr};
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
   virtual void validate() const override;
};
