#pragma once

#include <mach-o/loader.h>
#include <vector>
#include <list>

#include "lc.hh"
#include "segment.hh"
#include "parse.hh"
#include "trie.hh"

namespace MachO {

   template <Bits> class RebaseInfo;
   template <Bits> class BindInfo;
   template <Bits> class ExportInfo;

   template <Bits bits>
   class DyldInfo: public LinkeditCommand<bits> {
   public:
      dyld_info_command dyld_info;

      RebaseInfo<bits> *rebase;
      BindInfo<bits> *bind;
      std::vector<uint8_t> weak_bind;
      std::vector<uint8_t> lazy_bind;
      // std::vector<uint8_t> export_info;
      ExportInfo<bits> *export_info;

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
      static DyldInfo<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DyldInfo(img, offset, env);
      }

#if 0
      virtual DyldInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DyldInfo<opposite<bits>>(*this, env);
      }
#endif

   private:
      DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env);
#if 0
      DyldInfo(const DyldInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env), dyld_info(other.dyld_info),
         rebase(other.rebase->Transform(env)), bind(other.bind->Transform(env)),
         weak_bind(other.weak_bind), lazy_bind(other.lazy_bind), export_info(other.export_info) {}
#endif

      template <Bits b> friend class DyldInfo;
   };

   template <Bits bits>
   class RebaseNode {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;

      uint8_t type;
      const SectionBlob<bits> *blob;
      
      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      bool active() const { return blob == nullptr ? false : blob->active; }

      static RebaseNode<bits> *Parse(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
         return new RebaseNode(vmaddr, env, type);
      }

      RebaseNode<opposite<bits>> *Transform(TransformEnv<bits>& env) {
         return new RebaseNode<opposite<bits>>(*this, env);
      }
         
   private:
      RebaseNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type);
      RebaseNode(const RebaseNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class RebaseNode;
   };

   template <Bits bits>
   class RebaseInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Rebasees = std::list<RebaseNode<bits> *>;

      Rebasees rebasees;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      static RebaseInfo<bits> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                     ParseEnv<bits>& env) {
         return new RebaseInfo(img, offset, size, env);
      }
      
      RebaseInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new RebaseInfo<opposite<bits>>(*this, env);
      }
      
   private:
      RebaseInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      RebaseInfo(const RebaseInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      std::size_t do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type);
      std::size_t do_rebase_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                  uint8_t type, std::size_t skipping = 0);
      template <Bits> friend class RebaseInfo;
   };

   template <Bits bits>
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

      static BindNode<bits> *Parse(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend, std::size_t dylib, const char *sym, uint8_t flags) {
         return new BindNode(vmaddr, env, type, addend, dylib, sym, flags);
      }

      BindNode<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new BindNode<opposite<bits>>(*this, env);
      }
      
   private:
      BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
               std::size_t dylib, const char *sym, uint8_t flags);
      BindNode(const BindNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class BindNode;
   };
   template <Bits bits>
   class BindInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Bindees = std::list<BindNode<bits> *>;

      Bindees bindees;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      static BindInfo<bits> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                   ParseEnv<bits>& env)
      { return new BindInfo(img, offset, size, env); }

      BindInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new BindInfo<opposite<bits>>(*this, env);
      }

   private:
      BindInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      BindInfo(const BindInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      
      std::size_t do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                          std::size_t dylib, const char *sym, uint8_t flags);
      std::size_t do_bind_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                uint8_t type, ssize_t addend, std::size_t dylib, const char *sym,
                                uint8_t flags, ptr_t skipping = 0);
      template <Bits> friend class BindInfo;
   };

   template <Bits bits>
   class ExportNode {
   public:
      std::size_t flags;
      
      static ExportNode<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      std::size_t size() const;
      std::size_t Emit(Image& img, std::size_t offset) const;
      virtual ~ExportNode() {}
      
   protected:
      ExportNode(std::size_t flags): flags(flags) {}
      virtual std::size_t derived_size() const = 0;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const = 0;
   };

   template <Bits bits>
   class RegularExportNode: public ExportNode<bits> {
   public:
      const SectionBlob<bits> *value = nullptr;

      static RegularExportNode<bits> *Parse(const Image& img, std::size_t offset,
                                            std::size_t flags, ParseEnv<bits>& env) {
         return new RegularExportNode(img, offset, flags, env);
      }

   private:
      RegularExportNode(const Image& img, std::size_t offset, std::size_t flags,
                        ParseEnv<bits>& env);

      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;
   };

   template <Bits bits>
   class ReexportNode: public ExportNode<bits> {
   public:
      std::size_t libordinal;
      std::string name;

      static ReexportNode<bits> *Parse(const Image& img, std::size_t offset, std::size_t flags,
                                       ParseEnv<bits>& env) {
         return new ReexportNode<bits>(img, offset, flags, env);
      }
      
   private:
      ReexportNode(const Image& img, std::size_t offset, std::size_t flags, ParseEnv<bits>& env);
      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;
   };

   template <Bits bits>
   class StubExportNode: public ExportNode<bits> {
   public:
      std::size_t stuboff;
      std::size_t resolveroff;
      
      static StubExportNode<bits> *Parse(const Image& img, std::size_t offset, std::size_t flags,
                                         ParseEnv<bits>& env) {
         return new StubExportNode(img, offset, flags, env);
      }
      
   private:
      StubExportNode(const Image& img, std::size_t offset, std::size_t flags, ParseEnv<bits>& env);
      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;
   };

   template <Bits bits>
   class ExportTrie: public trie_map<char, std::string, std::unique_ptr<ExportNode<bits>>> {
   public:
      static ExportTrie<bits> Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      std::size_t content_size() const;
      void Emit(Image& img, std::size_t offset) const;
      
   private:
      using node = typename trie_map<char, std::string, std::unique_ptr<ExportNode<bits>>>::node;
      static node ParseNode(const Image& img, std::size_t offset, std::size_t start,
                            ParseEnv<bits>& env);
      static std::size_t NodeSize(const node& node);
      static std::size_t EmitNode(const node& node, Image& img, std::size_t offset,
                                  std::size_t start);
   };
   
   template <Bits bits>
   class ExportInfo {
   public:
      using Trie = ExportTrie<bits>;
      Trie trie;

      static ExportInfo<bits> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                     ParseEnv<bits>& env) {
         return new ExportInfo(img, offset, size, env);
      }
      void Emit(Image& img, std::size_t offset) const;

      std::size_t size() const;
      
   private:
      ExportInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
   };  
}
