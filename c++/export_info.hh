#pragma once

#include <string>

#include "types.hh"
#include "trie.hh"
#include "leb.hh"

namespace MachO {

   template <Bits bits>
   class ExportNode {
   public:
      std::size_t flags;
      
      static ExportNode<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      std::size_t size() const;
      std::size_t Emit(Image& img, std::size_t offset) const;
      virtual ~ExportNode() {}
      virtual ExportNode<opposite<bits>> *Transform(TransformEnv<bits>& env) const = 0;
      
   protected:
      ExportNode(std::size_t flags): flags(flags) {}
      ExportNode(const ExportNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         flags(other.flags) {}
         
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
      virtual RegularExportNode<opposite<bits>> *Transform(TransformEnv<bits>& env) const override
      { return new RegularExportNode<opposite<bits>>(*this, env); }

   private:
      RegularExportNode(const Image& img, std::size_t offset, std::size_t flags,
                        ParseEnv<bits>& env);
      RegularExportNode(const RegularExportNode<opposite<bits>>& other,
                        TransformEnv<opposite<bits>>& env): ExportNode<bits>(other, env) {
         env.resolve(other.value, &value);
      }

      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;

      template <Bits> friend class RegularExportNode;
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

      virtual ReexportNode<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new ReexportNode<opposite<bits>>(*this, env);
      }
      
   private:
      ReexportNode(const Image& img, std::size_t offset, std::size_t flags, ParseEnv<bits>& env);
      ReexportNode(const ReexportNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         ExportNode<bits>(other, env), libordinal(other.libordinal), name(other.name) {}
      
      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;

      template <Bits> friend class ReexportNode;
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
      
      virtual StubExportNode<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new StubExportNode<opposite<bits>>(*this, env);
      }

   private:
      StubExportNode(const Image& img, std::size_t offset, std::size_t flags, ParseEnv<bits>& env);
      StubExportNode(const StubExportNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         ExportNode<bits>(other, env), stuboff(other.stuboff), resolveroff(other.resolveroff) {}
      virtual std::size_t derived_size() const override;
      virtual std::size_t Emit_derived(Image& img, std::size_t offset) const override;

      template <Bits> friend class StubExportNode;
   };

   template <Bits bits>
   class ExportTrie: public trie_map<char, std::string, ExportNode<bits> *> {
   public:
      static ExportTrie<bits> Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      std::size_t content_size() const;
      void Emit(Image& img, std::size_t offset) const;

      ExportTrie<opposite<bits>> Transform(TransformEnv<bits>& env) const {
         return ExportTrie<opposite<bits>>(this->template transform<ExportNode<opposite<bits>> *>([&] (const ExportNode<bits> *value) -> ExportNode<opposite<bits>> * { return value->Transform(env); }));
      }

      template <typename... Args>
      ExportTrie(Args&&... args): trie_map<char, std::string, ExportNode<bits> *>(args...) {}

   private:
      using node = typename trie_map<char, std::string, ExportNode<bits> *>::node;
      static node ParseNode(const Image& img, std::size_t offset, std::size_t start,
                            ParseEnv<bits>& env);
      static std::size_t NodeSize(const node& node);
      static std::size_t EmitNode(const node& node, Image& img, std::size_t offset,
                                  std::size_t start);
      template <Bits> friend class ExportTrie;
      
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

      ExportInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new ExportInfo<opposite<bits>>(*this, env);
      }

      std::size_t size() const;
      
   private:
      ExportInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      ExportInfo(const ExportInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         trie(other.trie.Transform(env)) {}
      template <Bits> friend class ExportInfo;
   };
   
}
