#pragma once

#include "modify.hh"

struct ModifyCommand::Delete: Subcommand {
   struct Instruction;
   struct Segment;
   struct Symbol;
   
   virtual std::vector<char *> keylist() const override {
      return {"i", "inst", "instruction",
              "S", "seg", "segment",
              "sym", "symbol",
              nullptr};
   }

   virtual Functor *getop(int index) override;
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

struct ModifyCommand::Delete::Segment: Functor {
   std::string name;

   virtual int parse(char *option) override;
   virtual void operator()(MachO::MachO *macho) override;
};

struct ModifyCommand::Delete::Symbol: Functor {
   std::string name;

   virtual int parse(char *option) override;
   virtual void operator()(MachO::MachO *macho) override;
};
