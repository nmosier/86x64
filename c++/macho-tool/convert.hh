#pragma once

#include <optional>

#include "command.hh"

struct ConvertCommand: InOutCommand {
   std::optional<uint32_t> filetype;
   
   virtual std::string optusage() const override { return "[-h|-a <type>]"; }
   virtual const char *optstring() const override { return "ha:"; }

   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"archive", required_argument, nullptr, 'a'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;
   ConvertCommand(): InOutCommand("convert") {}
   void archive_EXECUTE_to_DYLIB(MachO::Archive<MachO::Bits::M64> *archive);
};
