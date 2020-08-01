#pragma once

#include <string>

#include "typeinfo.hh"

struct reg_group {
   const std::string reg_b;
   const std::string reg_w;
   const std::string reg_d;
   const std::string reg_q;
   
   const std::string& reg(reg_width width) const;
};

extern const reg_group rax;
extern const reg_group rdi;
extern const reg_group rsi;
extern const reg_group rdx;
extern const reg_group rcx;
extern const reg_group  r8;
extern const reg_group  r9;
extern const reg_group r11;
extern const reg_group r12;
extern const reg_group rsp;
extern const reg_group rbp;

struct Location {
   enum class Kind {REG, MEM, SSE};

   virtual Kind kind() const = 0;
   virtual std::string op(reg_width width) const = 0;
   virtual void push() = 0;
   virtual void pop() = 0;

   virtual Location *copy() const = 0;
   
   virtual ~Location() {}
};

struct MemoryLocation: Location {
   const reg_group& base;
   int index = 0;
   
   virtual Kind kind() const override { return Kind::MEM; }
   virtual std::string op(reg_width width) const override;
   std::string op() const;
   virtual void push() override;
   virtual void pop() override;

   inline MemoryLocation operator+(int offset) const { return MemoryLocation(base, index + offset); }
   inline MemoryLocation operator-(int offset) const { return operator+(-offset); }
   inline MemoryLocation& operator+=(int offset) { index += offset; return *this; }
   inline MemoryLocation& operator-=(int offset) { return operator+=(-offset); }

   // void align(int align);
   void align(CXType type, arch a);
   void align_field(CXType type, arch a);

   virtual MemoryLocation *copy() const override { return new MemoryLocation(base, index); }
   
   MemoryLocation(const reg_group& base, int index = 0): base(base), index(index) {}
};

struct RegisterLocation: Location {
   const reg_group& reg;
   
   virtual Kind kind() const override { return Kind::REG; }
   virtual std::string op(reg_width width) const override;
   virtual void push() override {}
   virtual void pop() override {}

   virtual RegisterLocation *copy() const override { return new RegisterLocation(reg); }

   RegisterLocation(const reg_group& reg): reg(reg) {}
};

struct SSELocation: Location {
   unsigned index;

   virtual Kind kind() const override { return Kind::SSE; }
   virtual std::string op(reg_width width) const override;
   virtual void push() override {}
   virtual void pop() override {}

   virtual SSELocation *copy() const override { return new SSELocation(index); }

   SSELocation(unsigned index): index(index) {}
};
