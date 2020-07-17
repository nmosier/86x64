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
   void add_rebase(std::size_t vmaddr, uint8_t type);
   Rebasify(): InOutCommand("rebasify") {}
};
