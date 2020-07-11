#pragma once

#include <optional>

#include "command.hh"

struct TransformCommand: InOutCommand {
   std::optional<MachO::Bits> bits;

   virtual const char *optstring() const override { return "hm:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"bits", required_argument, nullptr, 'm'},
              {0}};
   }
   virtual int opthandler(int optchar) override;
   virtual std::string optusage() const override { return "[-h | -m <bits>]"; }

   template <MachO::Bits bits> int workT(MachO::MachO *macho);
   virtual int work() override;

   TransformCommand(): InOutCommand("transform") {}
};
