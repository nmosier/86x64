#pragma once

#include "command.hh"

struct TranslateCommand: public InplaceCommand {
public:
   unsigned long offset = 0;

   virtual std::string optusage() const override { return "[-h|-o <offset>]"; }
   virtual const char *optstring() const override { return "ho:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"offset", required_argument, nullptr, 'o'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;
   TranslateCommand();
};
