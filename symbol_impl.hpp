#pragma once

#include <LIEF/LIEF.hpp>

class FunctionSymbol {
public:
   using Symbol = LIEF::MachO::Symbol;
   using content_t = LIEF::MachO::Section::content_t;

   Symbol& symbol() { return symbol_; }
   const Symbol& symbol() const { return symbol_; }

   content_t& content() { return content_; }
   const content_t& content() const { return content_; }

   FunctionSymbol(Symbol& symbol, const content_t& content): symbol_(symbol), content_(content) {}
   
private:
   Symbol& symbol_;
   content_t content_;
};
