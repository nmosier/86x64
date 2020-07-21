#pragma once

#include "modify.hh"

struct ModifyCommand::Start: Functor {
   std::size_t vmaddr;
   
   virtual int parse(char *option) override;
   virtual void operator()(MachO::MachO *macho) override;
};
