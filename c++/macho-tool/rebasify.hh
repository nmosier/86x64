#pragma once

#include "command.hh"

struct Rebasify: InOutCommand {

   virtual const char *optstring() const override { return "h"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {0}};
   }
   virtual int opthandler(int optchar) override;
   virtual std::string optusage() const override { return "[-h]"; }
   virtual int work() override;
   Rebasify(): InOutCommand("rebasify") {}
};
