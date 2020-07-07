#pragma once

#include <list>

#include "command.hh"
#include "loc.hh"

struct ModifyCommand: public InOutCommand {
   int help = 0;

   struct Insert;
   struct Delete;
   struct Start;

   virtual const char *optstring() const override { return "hi:d:s:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"insert", required_argument, nullptr, 'i'},
              {"delete", required_argument, nullptr, 'd'},
              {"start", required_argument, nullptr, 's'},
              {0}};
   }
   virtual int opthandler(int optchar) override;

   std::list<std::unique_ptr<Functor>> operations;

   virtual std::string optusage() const override {
      return "[-h | -i (vmaddr=<vmaddr>|offset=<offset>),bytes=<count>,[before|after] | -d (vmaddr=<vmaddr>|offset=<offset>) | -s <vmaddr>]";
   }

   virtual int work() override;
   ModifyCommand(): InOutCommand("modify") {}
};


