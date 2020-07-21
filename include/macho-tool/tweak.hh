#pragma once

#include "command.hh"
#include "util.hh"

struct TweakCommand: InplaceCommand {
   Flags<uint32_t> mach_header_flags;
   std::optional<uint32_t> mach_header_filetype;
   
   int pie = -1;
   
   virtual std::string optusage() const override { return "[-h|-f <flag>[,<flag>]*]"; }
   virtual const char *optstring() const override { return "hf:t:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"flags", required_argument, nullptr, 'f'},
              {"type", required_argument, nullptr, 't'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;


   TweakCommand();
   
};
