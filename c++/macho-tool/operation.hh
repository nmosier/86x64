#pragma once

#include <vector>

#include "macho.hh"

struct Operation {
   virtual void operator()(MachO::MachO *macho) = 0;
   virtual std::vector<char *> keylist() const = 0;
   virtual int subopthandler(int index) = 0;

   int parsesubopts(char *option);
};
