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
      
      virtual Symtab<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Symtab<opposite<bits>>(*this, env);
      }

   private:
      Symtab(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      Symtab(const Symtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class Symtab;
   };

   template <Bits bits>
   class Nlist {
   public:
      static std::size_t size() { return sizeof(nlist_t<bits>); }

      nlist_t<bits> nlist;
      const String<bits> *string;
      const SectionBlob<bits> *value;

      static Nlist<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                const std::unordered_map<std::size_t, String<bits> *>& off2str) {
         return new Nlist(img, offset, env, off2str);
      }

      void Build();
      void Emit(Image& img, std::size_t offset) const;
      Nlist<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Nlist<opposite<bits>>(*this, env);
      }
      
   private:
      Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
            const std::unordered_map<std::size_t, String<bits> *>& off2str);
      Nlist(const Nlist<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class Nlist;
   };

   template <Bits bits>
   class String {
   public:
      std::string str;
      std::size_t offset; /*!< offset inside string table */

      std::size_t size() const { return str.size() + 1; }

      static String<bits> *Parse(const Image& img, std::size_t offset, std::size_t maxlen) {
         return new String(img, offset, maxlen);
      }

      void Build(BuildEnv<bits>& env);
      void Emit(Image& img, std::size_t offset) const;

      String<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new String<opposite<bits>>(*this, env);
      }
      
   private:
      String(const Image& img, std::size_t offset, std::size_t maxlen);
      String(const String<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class String;
   };

   template <Bits bits>
   class Dysymtab: public LinkeditCommand<bits> {
   public:
      dysymtab_command dysymtab;
      std::vector<uint32_t> indirectsyms;

      virtual uint32_t cmd() const override { return dysymtab.cmd; }      
      virtual std::size_t size() const override { return sizeof(dysymtab_command); }

      static Dysymtab<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new Dysymtab(img, offset, env);
      }
      
      template <typename... Args>
      static Dysymtab<bits> *Parse(Args&&... args) { return new Dysymtab(args...); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
      virtual Dysymtab<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Dysymtab<opposite<bits>>(*this, env);
      }

   private:
      Dysymtab(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      Dysymtab(const Dysymtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env), dysymtab(other.dysymtab),
         indirectsyms(other.indirectsyms) {}
      template <Bits> friend class Dysymtab;
   };   
   
}
