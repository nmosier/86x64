#pragma once

#include "command.hh"

struct NoopCommand: InOutCommand {
   int help = 0;

   virtual std::string optusage() const override { return "[-h]"; }

   virtual const char *optstring() const override { return "h"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;

   NoopCommand(): InOutCommand("noop") {}
};
