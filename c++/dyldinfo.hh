#pragma once

#include <mach-o/loader.h>
#include <vector>
#include <list>

#include "lc.hh"
#include "segment.hh"
#include "parse.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class DyldInfo: public LinkeditCommand<bits> {
   public:
      dyld_info_command dyld_info;

      RebaseInfo<bits> *rebase;
      BindInfo<bits, false> *bind;
      std::vector<uint8_t> weak_bind;
      // std::vector<uint8_t> lazy_bind;
      BindInfo<bits, true> *lazy_bind;
      ExportInfo<bits> *export_info;

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
      static DyldInfo<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DyldInfo(img, offset, env);
      }

      virtual DyldInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DyldInfo<opposite<bits>>(*this, env);
      }

   private:
      DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DyldInfo(const DyldInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env),
         dyld_info(other.dyld_info),
         rebase(other.rebase->Transform(env)),
         bind(other.bind->Transform(env)),
         weak_bind(other.weak_bind),
         lazy_bind(other.lazy_bind->Transform(env)),
         export_info(other.export_info->Transform(env)) {}

      template <Bits b> friend class DyldInfo;
   };

   template <Bits bits, bool lazy>
   class BindNode {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      uint8_t type;
      ssize_t addend;
      const DylibCommand<bits> *dylib;
      std::string sym;
      uint8_t flags;
      const SectionBlob<bits> *blob;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;
      bool active() const { return blob == nullptr ? false : blob->active; }

      static BindNode<bits, lazy> *Parse(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend, std::size_t dylib, const char *sym, uint8_t flags) {
         return new BindNode(vmaddr, env, type, addend, dylib, sym, flags);
      }

      BindNode<opposite<bits>, lazy> *Transform(TransformEnv<bits>& env) const {
         return new BindNode<opposite<bits>, lazy>(*this, env);
      }
      
   private:
      BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
               std::size_t dylib, const char *sym, uint8_t flags);
      BindNode(const BindNode<opposite<bits>, lazy>& other, TransformEnv<opposite<bits>>& env);
      template <Bits, bool> friend class BindNode;
   };

   template <Bits bits, bool lazy>
   class BindInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Bindees = std::list<BindNode<bits, lazy> *>;

      Bindees bindees;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      static BindInfo<bits, lazy> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                   ParseEnv<bits>& env)
      { return new BindInfo(img, offset, size, env); }

      BindInfo<opposite<bits>, lazy> *Transform(TransformEnv<bits>& env) const {
         return new BindInfo<opposite<bits>, lazy>(*this, env);
      }

   private:
      BindInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      BindInfo(const BindInfo<opposite<bits>, lazy>& other, TransformEnv<opposite<bits>>& env);
      
      std::size_t do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                          std::size_t dylib, const char *sym, uint8_t flags);
      std::size_t do_bind_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                uint8_t type, ssize_t addend, std::size_t dylib, const char *sym,
                                uint8_t flags, ptr_t skipping = 0);
      template <Bits, bool> friend class BindInfo;
   };

}
