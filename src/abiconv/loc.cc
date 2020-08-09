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
      index += 8;
   }
}

void MemoryLocation::pop() {
   if (base.reg_q == "rsp") {
      index -= 8;
   }
}

void MemoryLocation::align(CXType type, arch a) {
   const size_t align = alignof_type(type, a);
   index = align_up<int>(index, align);
}

const reg_group rax = {"al",   "ax",   "eax",  "rax"};
const reg_group rdi = {"dil",  "di",   "edi",  "rdi"};
const reg_group rsi = {"sil",  "si",   "esi",  "rsi"};
const reg_group rdx = {"dl",   "dx",   "edx",  "rdx"};
const reg_group rcx = {"cl",   "cx",   "ecx",  "rcx"};
const reg_group r8  = {"r8b",  "r8w",  "r8d",  "r8"};
const reg_group r9  = {"r9b",  "r9w",  "r9d",  "r9"};
const reg_group r10 = {"r10b", "r10w", "r10d", "r10"};
const reg_group r11 = {"r11b", "r11w", "r11d", "r11"};
const reg_group r12 = {"r12b", "r12w", "r12d", "r12"};
const reg_group rsp = {"spl",  "sp",   "esp",  "rsp"};
const reg_group rbp = {"bpl",  "bp",   "ebp",  "rbp"};

const std::string& reg_group::reg(reg_width width) const {
   switch (width) {
   case reg_width::B: return reg_b;
   case reg_width::W: return reg_w;
   case reg_width::D: return reg_d;
   case reg_width::Q: return reg_q;
   default: throw std::invalid_argument("bad register width");
   }
}

void MemoryLocation::align_field(CXType type, arch a) {
   size_t align = alignof_type(type, a);
   if (align == 8 && a == arch::i386) {
      align = 4;
   }
   index = align_up<int>(index, align);
}
