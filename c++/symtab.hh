#pragma once

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "lc.hh"

namespace MachO {

   template <Bits bits> class String;

   template <Bits bits>
   class Symtab: public LoadCommand<bits> {
   public:
      using Nlists = std::vector<Nlist<bits> *>;
      using Strings = std::vector<String<bits> *>;
      
      symtab_command symtab;
      Nlists syms;
      Strings strs;
      
      virtual uint32_t cmd() const override { return symtab.cmd; }
      virtual std::size_t size() const override { return sizeof(symtab); }

      template <typename... Args>
      static Symtab<bits> *Parse(Args&&... args) { return new Symtab(args...); }
      virtual void Build(BuildEnv<bits>& env) override;
      
   private:
      Symtab(const Image& img, std::size_t offset);
      
   };

   template <Bits bits>
   class Nlist {
   public:
      using nlist_t = select_type<bits, struct nlist, struct nlist_64>;

      static std::size_t size() { return sizeof(nlist_t); }

      nlist_t nlist;
      String<bits> *string;
      
      template <typename... Args>
      static Nlist<bits> *Parse(Args&&... args) { return new Nlist(args...); }
      void Build();
      
   private:
      Nlist(const Image& img, std::size_t offset,
            const std::unordered_map<std::size_t, String<bits> *>& off2str);
   };

   template <Bits bits>
   class String {
   public:
      std::string str;
      std::size_t offset; /*!< offset inside string table */

      std::size_t size() const { return str.size() + 1; }

      template <typename... Args>
      static String<bits> *Parse(Args&&... args) { return new String(args...); }

      void Build(BuildEnv<bits>& env);
      
   private:
      String(const Image& img, std::size_t offset, std::size_t maxlen);
   };

   template <Bits bits>
   class Dysymtab: public LoadCommand<bits> {
   public:
      dysymtab_command dysymtab;
      std::vector<uint32_t> indirectsyms;

      virtual uint32_t cmd() const override { return dysymtab.cmd; }      
      virtual std::size_t size() const override { return sizeof(dysymtab_command); }

      template <typename... Args>
      static Dysymtab<bits> *Parse(Args&&... args) { return new Dysymtab(args...); }
      
   private:
      Dysymtab(const Image& img, std::size_t offset);
   };   
   
}
