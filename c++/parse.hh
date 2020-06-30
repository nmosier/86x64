#pragma once

#include "macho-fwd.hh"
#include "loc.hh"
#include "archive.hh"
#include "resolve.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;

   template <typename T>
   class CountResolver {
   public:
      void add(T *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      CountResolver(): id(0) {}
      
   private:
      Resolver<std::size_t, T> resolver;
      unsigned id;
   };

   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;
      
      Resolver<std::size_t, SectionBlob<bits>> vmaddr_resolver;
      Resolver<std::size_t, SectionBlob<bits>> offset_resolver;
      CountResolver<DylibCommand<bits>> dylib_resolver;
      CountResolver<Segment<bits>> segment_resolver;
      Segment<bits> *current_segment;
      
      ParseEnv(Archive<bits>& archive): archive(archive), current_segment(nullptr) {}
      
   private:
      
   };
   
}
