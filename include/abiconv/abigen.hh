#pragma once

#include <string>
#include <unordered_set>

#include "typeinfo.hh"

using Symbols = std::unordered_set<std::string>;

struct reg_group {
   const std::string reg_b;
   const std::string reg_w;
   const std::string reg_d;
   const std::string reg_q;
   
   const std::string& reg(reg_width width) const;
};
