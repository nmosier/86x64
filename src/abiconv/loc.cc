#include <sstream>

#include "loc.hh"

std::string MemoryLocation::op(reg_width width) const {
   std::stringstream ss;
   ss << reg_width_to_str(width) << " " << "[" << base.reg_q << "+" << index << "]";
   return ss.str();
}

std::string RegisterLocation::op(reg_width width) const {
   return reg.reg(width);
}

std::string SSELocation::op(reg_width width) const {
   std::stringstream ss;
   ss << "xmm" << index;
   return ss.str();
}

void MemoryLocation::push() {
   if (base.reg_q == "rsp") {
      index -= 8;
   }
}

void MemoryLocation::pop() {
   if (base.reg_q == "rsp") {
      index += 8;
   }
}
