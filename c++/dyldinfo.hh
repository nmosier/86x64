#pragma once

#include <mach-o/loader.h>
#include <vector>
#include <list>

#include "macho-fwd.hh"
#include "lc.hh"
#include "segment.hh"
#include "parse.hh"

namespace MachO {

   template <Bits bits> class RebaseInfo;

   template <Bits bits>
   class DyldInfo: public LoadCommand<bits> {
   public:
      dyld_info_command dyld_info;

      RebaseInfo<bits> *rebase;
      std::vector<uint8_t> bind;
      std::vector<uint8_t> weak_bind;
      std::vector<uint8_t> lazy_bind;
      std::vector<uint8_t> export_info;

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }

      template <typename... Args>
      static DyldInfo<bits> *Parse(Args&&... args) { return new DyldInfo(args...); }
      
   private:
      DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class RebaseInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;

      // std::list<const LazySymbolPointer<bits> *> rebasees;
      std::list<const SectionBlob<bits> *> rebasees;
      
      template <typename... Args>
      static RebaseInfo<bits> *Parse(Args&&... args) { return new RebaseInfo(args...); }
   private:
      RebaseInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      std::size_t do_rebase(std::size_t vmaddr, ParseEnv<bits>& env);
      std::size_t do_rebase_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                  std::size_t skipping = 0);
   };

   template <Bits bits>
   class BindInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;

      std::list<const SectionBlob<bits> *> bindees;

      template <typename... Args>
      static BindInfo<bits> *Parse(Args&&... args) { return new BindInfo(args...); }
      
   private:
      BindInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
   };
   
}
