#pragma once

#include <list>
#include <optional>
#include <vector>

#include <LIEF/LIEF.hpp>
extern "C" {
#include <xed/xed-interface.h>
}

class DecodedFunction {
public:
   using Symbol = LIEF::MachO::Symbol;
   using Binary = LIEF::MachO::Binary;
   using Section = LIEF::MachO::Section;
   using content_t = std::list<xed_decoded_inst_t>;

   bool has_symbol() const { return (bool) symbol_; }
   Symbol *symbol() { return *symbol_; }
   const Symbol *symbol() const { return *symbol_; }

   content_t& insts() { return insts_; }
   const content_t& insts() const { return insts_; }

   DecodedFunction(std::optional<Symbol *> symbol, const content_t& insts):
      symbol_(symbol), insts_(insts) {}
   DecodedFunction(std::optional<Symbol *> symbol, const xed_uint8_t *itext,
                   const unsigned int bytes):
      symbol_(symbol), insts_(content_from_itext(itext, bytes)) {}
   DecodedFunction(std::optional<Symbol *> symbol, uint64_t addr, uint64_t bytes,
                   const Binary& binary):
      symbol_(symbol), insts_(content_from_addr(addr, bytes, binary)) {}

   static content_t content_from_itext(const xed_uint8_t *itext, uint64_t bytes);
   
   /**
    * Get content from function address relative to beginning of __text section.
    * @param addr address, relative in __text
    * @param bytes length of function content
    * @return list of decoded instructions
    */
   static content_t content_from_addr(uint64_t addr, uint64_t bytes, const Binary& binary);

private:
   std::optional<Symbol *> symbol_;
   content_t insts_;

};

class DecodedFunctions {
public:
   using Functions = std::list<DecodedFunction>;
   using Binary = LIEF::MachO::Binary;

   Functions& functions() { return functions_; }
   const Functions& functions() const { return functions_; }

   DecodedFunctions(const Functions& functions): functions_(functions) {}
   DecodedFunctions(const Binary& binary);
   
   
private:
   Functions functions_;
};
