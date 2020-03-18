#pragma once

#include <list>

#include <LIEF/LIEF.hpp>
extern "C" {
#include <xed/xed-interface.h>
}

class DecodedFunction {
public:
   using Symbol = LIEF::Function;
   using content_t = std::list<xed_decoded_inst_t>;
   
   Symbol& symbol() { return symbol_; }
   const Symbol& symbol() const { return symbol_; }

   content_t& insts() { return insts_; }
   const content_t& insts() const { return insts_; }

   DecodedFunction(Symbol& symbol, const content_t& insts): symbol_(symbol), insts_(insts) {}
   DecodedFunction(Symbol& symbol, const xed_uint8_t *itext, const unsigned int bytes):
      symbol_(symbol), insts_(content_from_itext(itext, bytes)) {}
   DecodedFunction(Symbol& symbol, const LIEF::MachO::Binary& binary):
      symbol_(symbol), insts_(content_from_binary(symbol, binary)) {}

   static content_t content_from_itext(const xed_uint8_t *itext, unsigned int bytes);
   static content_t content_from_binary(const Symbol& symbol, const LIEF::MachO::Binary& binary);
   
private:
   Symbol& symbol_;
   content_t insts_;

};
