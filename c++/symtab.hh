#pragma once

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "lc.hh"

namespace MachO {

   template <Bits bits> class String;

   template <Bits bits>
   class Symtab: public LinkeditCommand<bits> {
   public:
      using Nlists = std::vector<Nlist<bits> *>;
      using Strings = std::vector<String<bits> *>;
      
      symtab_command symtab;
      Nlists syms;
      Strings strs;
      
      virtual uint32_t cmd() const override { return symtab.cmd; }
      virtual std::size_t size() const override { return sizeof(symtab); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      template <typename... Args>
      static Symtab<bits> *Parse(Args&&... args) { return new Symtab(args...); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual std::size_t content_size() const override;
      
   private:
      Symtab(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      
   };

   template <Bits bits>
   class Nlist {
   public:
      using nlist_t = select_type<bits, struct nlist, struct nlist_64>;

      static std::size_t size() { return sizeof(nlist_t); }

      nlist_t nlist;
      String<bits> *string;
      const SectionBlob<bits> *value;
      
      template <typename... Args>
      static Nlist<bits> *Parse(Args&&... args) { return new Nlist(args...); }
      void Build();
      void Emit(Image& img, std::size_t offset) const;
      
   private:
      Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
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
      void Emit(Image& img, std::size_t offset) const;
      
   private:
      String(const Image& img, std::size_t offset, std::size_t maxlen);
   };

   template <Bits bits>
   class Dysymtab: public LinkeditCommand<bits> {
   public:
      dysymtab_command dysymtab;
      std::vector<uint32_t> indirectsyms;

      virtual uint32_t cmd() const override { return dysymtab.cmd; }      
      virtual std::size_t size() const override { return sizeof(dysymtab_command); }

      template <typename... Args>
      static Dysymtab<bits> *Parse(Args&&... args) { return new Dysymtab(args...); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
   private:
      Dysymtab(const Image& img, std::size_t offset);
   };   
   
}
