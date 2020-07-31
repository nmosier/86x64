#include <sstream>

#include "loc.hh"
#include "typeconv.hh"

std::string MemoryLocation::op() const {
   std::stringstream ss;
   ss << " " << "[" << base.reg_q << "+" << index << "]";
   return ss.str();
}

std::string MemoryLocation::op(reg_width width) const {
   return std::string(reg_width_to_str(width)) + " " + op();
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

void MemoryLocation::align(CXType type, arch a) {
   const size_t align = alignof_type(type, a);
   index = align_up<int>(index, align);
}
