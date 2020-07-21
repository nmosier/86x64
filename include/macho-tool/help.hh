#pragma once

#include "command.hh"

struct HelpCommand: Command {
   virtual std::string optusage() const override { return ""; }
   virtual std::string subusage() const override { return ""; }

   virtual const char *optstring() const override { return ""; }
   virtual std::vector<option> longopts() const override { return {}; }
   virtual int opthandler(int optchar) override { return 1; }
   virtual void arghandler(int argc, char *argv[]) override {}

   virtual int work() override {
      usage(std::cout);
      return 0;
   }

   HelpCommand(): Command("help") {}
};
