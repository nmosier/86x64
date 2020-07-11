#include "export_info.hh"
#include "parse.hh"
#include "transform.hh"
#include "section_blob.hh"

namespace MachO {

   template <Bits bits>
   ExportInfo<bits>::ExportInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      if (size > 0) {
         trie = ExportTrie<bits>::Parse(img, offset, env);
      }
   }

   template <Bits bits>
   RegularExportNode<bits>::RegularExportNode(const Image& img, std::size_t offset,
                                              std::size_t flags, ParseEnv<bits>& env):
      ExportNode<bits>(flags)
   {
      std::size_t value_offset;
      offset += leb128_decode(img, offset, value_offset);
      env.offset_resolver.resolve(value_offset, &value);
   }

   template <Bits bits>
   ReexportNode<bits>::ReexportNode(const Image& img, std::size_t offset, std::size_t flags,
                                    ParseEnv<bits>& env): ExportNode<bits>(flags) {
      offset += leb128_decode(img, offset, libordinal);
      name = std::string(&img.at<char>(offset));
      offset += name.size() + 1;
   }

   template <Bits bits>
   StubExportNode<bits>::StubExportNode(const Image& img, std::size_t offset, std::size_t flags,
                                        ParseEnv<bits>& env): ExportNode<bits>(flags) {
      offset += leb128_decode(img, offset, stuboff);
      offset += leb128_decode(img, offset, resolveroff);
   }


   template <Bits bits>
   std::size_t RegularExportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max());
   }

   template <Bits bits>
   std::size_t ReexportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max()) + (name.size() + 1);
   }

   template <Bits bits>
   std::size_t StubExportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max()) * 2;
   }
   
   template <Bits bits>
   ExportTrie<bits> ExportTrie<bits>::Parse(const Image& img, std::size_t offset,
                                            ParseEnv<bits>& env) {
      ExportTrie<bits> t;
      t.root = ParseNode(img, offset, offset, env);
      return t;
   }

   template <Bits bits>
   ExportNode<bits> *ExportNode<bits>::Parse(const Image& img, std::size_t offset,
                                             ParseEnv<bits>& env) {
      std::size_t flags;
      offset += leb128_decode(img, offset, flags);
      
      if ((flags & EXPORT_SYMBOL_FLAGS_REEXPORT)) {
         return ReexportNode<bits>::Parse(img, offset, flags, env);
      } else if ((flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER)) {
         return StubExportNode<bits>::Parse(img, offset, flags, env);
      } else {
         return RegularExportNode<bits>::Parse(img, offset, flags, env);
      }
   }

   template <Bits bits> typename ExportTrie<bits>::node
   ExportTrie<bits>::ParseNode(const Image& img, std::size_t offset, std::size_t start,
                               ParseEnv<bits>& env) {

      /* decode info */
      std::size_t size;
      offset += leb128_decode(img, offset, size);
 
      node curnode;
      if (size > 0) {
         curnode.value = ExportNode<bits>::Parse(img, offset, env);
      }
      offset += size;
      
      
      const uint8_t nedges = img.at<uint8_t>(offset++);
      for (uint8_t i = 0; i < nedges; ++i) {
         const char *sym = &img.at<char>(offset);
         offset += strlen(sym) + 1;

         std::size_t edge_diff;
         offset += leb128_decode(img, offset, edge_diff);

         node *subnode = &curnode;
         while (*sym) {
            auto result = subnode->children.emplace(*sym++, node());
            subnode = &result.first->second;
         }
         
         *subnode = ParseNode(img, edge_diff + start, start, env);
      }
      
      return curnode;
   }

   template <Bits bits>
   std::size_t ExportInfo<bits>::size() const {
      return trie.content_size();
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::content_size() const {
      return NodeSize(this->root);
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::NodeSize(const node& node) {
      std::size_t size = 0;

      if (node.value) {
         size += (*node.value)->size(); /* size of info */
      }
      size += leb128_size(size); /* size of size of info */
      ++size; /* nedges */

      for (auto& child : node.children) {
         size += 2;
         size += leb128_size(std::numeric_limits<std::size_t>::max()); /* max size of offset */
         size += NodeSize(child.second);
      }

      return size;
   }

   template <Bits bits>
   void ExportTrie<bits>::Emit(Image& img, std::size_t offset) const {
      EmitNode(this->root, img, offset, offset);
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::EmitNode(const node& node, Image& img, std::size_t offset,
                                          std::size_t start) {
      /* emit node info size */
      std::size_t info_size = node.value ? (*node.value)->size() : 0;
      std::size_t info_off;
      std::size_t info_actual_size = info_size;
      do {
         info_size = info_actual_size;
         info_off = offset;
         info_off += leb128_encode(img, info_off, info_size);
         if (node.value) {
            info_actual_size = (*node.value)->Emit(img, info_off);
         } else {
            info_actual_size = 0;
         }
         info_off += info_actual_size;
      } while (info_actual_size != info_size);
      offset = info_off;

      /* emit edge count */
      const uint8_t nedges = node.children.size();
      img.at<uint8_t>(offset++) = nedges;
      
      /* compute offset past edges */
      std::size_t offset_past_edges = offset +
         nedges * (2 /* c + '\0' */ + leb128_size(std::numeric_limits<std::size_t>::max()));

      /* emit edges & children */
      for (auto& child : node.children) {
         img.at<char>(offset++) = child.first;
         img.at<char>(offset++) = '\0';
         offset += leb128_encode(img, offset, offset_past_edges - start);
         offset_past_edges = EmitNode(child.second, img, offset_past_edges, start);
      }

      return offset_past_edges;
   }

   template <Bits bits>
   std::size_t ExportNode<bits>::Emit(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, flags);
      offset += Emit_derived(img, offset);
      return offset - start;
   }

   template <Bits bits>
   void ExportInfo<bits>::Emit(Image& img, std::size_t offset) const {
      return trie.Emit(img, offset);
   }

   template <Bits bits>
   std::size_t ExportNode<bits>::size() const {
      return leb128_size(flags) + derived_size();
   }

   template <Bits bits>
   std::size_t RegularExportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, value ? value->loc.offset : 0);
      return offset - start;
   }
   
   template <Bits bits>
   std::size_t ReexportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, libordinal);
      img.copy(offset, name.c_str(), name.size() + 1);
      offset += name.size() + 1;
      return offset - start;
   }

   template <Bits bits>
   std::size_t StubExportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, stuboff);
      offset += leb128_encode(img, offset, resolveroff);
      return offset - start;
   }

   template class ExportInfo<Bits::M32>;
   template class ExportInfo<Bits::M64>;

}
