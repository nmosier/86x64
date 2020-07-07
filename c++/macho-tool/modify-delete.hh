#pragma once

#include "modify.hh"

struct ModifyCommand::Delete: Subcommand {
   struct Instruction;

   virtual std::vector<char *> keylist() const override {
      return {"i", "inst", "instruction", nullptr};
   }

   virtual Operation *getop(int index) override;
};

struct ModifyCommand::Delete::Instruction: Operation {
   enum class LocationKind {OFFSET, VMADDR} kind;
   std::size_t loc;

   virtual std::vector<char *> keylist() const override {
      return {"vmaddr", "offset", nullptr};
   }
   
   virtual void operator()(MachO::MachO *macho) override;
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override {}
};
