#pragma once

#include <list>

#include <LIEF/LIEF.hpp>
extern "C" {
#include <xed/xed-interface.h>
}

class Function {
public:
   using Symbol = LIEF::MachO::Symbol;
   using content_t = std::list<xed_decoded_inst_t>;

   Symbol& symbol() { return symbol_; }
   const Symbol& symbol() const { return symbol_; }

   content_t& insts() { return insts_; }
   const content_t& insts() const { return insts_; }

   Function(Symbol& symbol, const content_t& insts): symbol_(symbol), insts_(insts) {}
   Function(Symbol& symbol, const xed_uint8_t *itext, const unsigned int bytes);
   
private:
   Symbol& symbol_;
   content_t insts_;
};
