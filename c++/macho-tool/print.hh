#pragma once

#include <list>

#include "command.hh"

struct PrintCommand: InplaceCommand {
   static constexpr int REBASE = 256;
   
   virtual const char *optstring() const override { return "h"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"rebase", no_argument, nullptr, REBASE},
              {0}};
   }
   virtual int opthandler(int optchar) override;

   std::list<int> ops;

   virtual std::string optusage() const override { return "[-h | --rebase]"; }

   virtual int work() override;

   template <MachO::Bits bits> int workT(const MachO::MachO *macho); 
   
   PrintCommand();

   template <MachO::Bits bits> void print_REBASE(const MachO::Archive<bits> *macho);
   
};
