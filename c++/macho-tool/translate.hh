#pragma once

#include <optional>

#include "command.hh"

struct TranslateCommand: InplaceCommand {
   struct Offset;
   struct LoadDylib;
   
   std::unique_ptr<Functor> op;

   unsigned long offset = 0;

   static constexpr int LOAD_DYLIB = 256;

   virtual std::string optusage() const override { return "[-h|-o <offset>]"; }
   virtual const char *optstring() const override { return "ho:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"offset", required_argument, nullptr, 'o'},
              {"load-dylib", required_argument, nullptr, LOAD_DYLIB},
              {0}};
   }
   
   virtual int opthandler(int optchar) override;
   virtual int work() override;
   TranslateCommand();
};

struct TranslateCommand::Offset: Functor {
   std::optional<std::size_t> offset;

   virtual int parse(char *option) override;
   virtual void operator()(MachO::MachO *macho) override;

   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
};
