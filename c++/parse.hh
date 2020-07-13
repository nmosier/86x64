#pragma once

#include <map>
#include <unordered_map>

#include "loc.hh"
#include "types.hh"
#include "resolve.hh"
#include "region.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;
   template <Bits bits> class RelocBlob;
   
   template <typename T>
   class CountResolver {
   public:
      void add(T *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      CountResolver(): id(0) {}
      
   private:
      Resolver<std::size_t, T, false> resolver;
      unsigned id;
   };

   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;
      
      Resolver<std::size_t, SectionBlob<bits>, true> vmaddr_resolver;
      Resolver<std::size_t, SectionBlob<bits>, true> offset_resolver;
      std::unordered_map<std::size_t, RelocBlob<bits> *> relocs;
      // Resolver<std::size_t, RelocBlob<bits>> reloc_resolver; /*!< resolves by vmaddr */
      CountResolver<DylibCommand<bits>> dylib_resolver;
      CountResolver<Segment<bits>> segment_resolver;
      CountResolver<Section<bits>> section_resolver;
      Segment<bits> *current_segment = nullptr;
      Section<bits> *current_section = nullptr;
      Regions data_in_code;

      /* vmaddr-to-placeholder map */
      using TodoPlaceholders = std::map<std::size_t, Placeholder<bits> *>;
      TodoPlaceholders placeholders;

      Placeholder<bits> *add_placeholder(std::size_t vmaddr);
      void do_resolve();
      
      
      ParseEnv(Archive<bits>& archive):
         archive(archive), current_segment(nullptr), current_section(nullptr) {}
      
   private:
      
   };
   
}
