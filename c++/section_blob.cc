#include <mach-o/x86_64/reloc.h>
#include <typeinfo>

#include "section_blob.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"

namespace MachO {

   template <Bits bits>
   Immediate<bits>::Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env,
                              bool is_pointer):
      SectionBlob<bits>(loc, env), value(img.at<uint32_t>(loc.offset)), pointee(nullptr) 
   {
      if (is_pointer) {
         env.vmaddr_resolver.resolve(loc.vmaddr, &pointee);
      }
   }
   
   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const Location& loc, ParseEnv<bits>& env, bool add_to_map):
      segment(env.current_segment), section(env.current_section), loc(loc)
   {
      if (loc.vmaddr == 0x1f9d) {
         fprintf(stderr, "%s\n", typeid(*this).name());
      }
      
      
      if (add_to_map) {
         env.vmaddr_resolver.add(loc.vmaddr, this);
         env.offset_resolver.add(loc.offset, this);
      }
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const Image& img, const Location& loc,
                                              ParseEnv<bits>& env):
      SymbolPointer<bits>(loc, env)
   {
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      std::size_t targetaddr = img.at<ptr_t>(loc.offset);
      env.vmaddr_resolver.resolve(targetaddr, (const SectionBlob<bits> **) &pointee);
   }

   template <Bits bits>
   void SectionBlob<bits>::Build(BuildEnv<bits>& env) {
      env.allocate(active ? size() : 0, loc);
   }

   template <Bits bits>
   void DataBlob<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint8_t>(offset) = data;
   }

   template <Bits bits>
   void SymbolPointer<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<ptr_t>(offset) = raw_data();
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const SectionBlob<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env): segment(nullptr) {
      env.add(&other, this);
      env.resolve(other.segment, &segment);
   }


   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                                              TransformEnv<opposite<bits>>& env):
      SymbolPointer<bits>(other, env), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   Immediate<bits>::Immediate(const Immediate<opposite<bits>>& other,
                              TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), value(other.value), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   void Immediate<bits>::Emit(Image& img, std::size_t offset) const {
      uint32_t value;
      if (pointee) {
         value = pointee->loc.vmaddr;
      } else {
         value = this->value;
      }
      img.at<uint32_t>(offset) = value;
   }

   template <Bits bits>
   DataBlob<bits>::DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), data(img.at<uint8_t>(loc.offset)) {}

   template <Bits bits>
   DataBlob<bits>::DataBlob(const DataBlob<opposite<bits>>& other,
                            TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), data(other.data) {}

   template <Bits bits>
   RelocBlob<bits> *RelocBlob<bits>::Parse(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env, uint8_t type) {
      switch (type) {
      case X86_64_RELOC_UNSIGNED: return RelocUnsignedBlob<bits>::Parse(img, loc, env);
      case X86_64_RELOC_BRANCH:	 return RelocBranchBlob<bits>::Parse(img, loc, env);

      case X86_64_RELOC_SIGNED:		
      case X86_64_RELOC_GOT_LOAD:		
      case X86_64_RELOC_GOT:			
      case X86_64_RELOC_SUBTRACTOR:	
      case X86_64_RELOC_SIGNED_1:		
      case X86_64_RELOC_SIGNED_2:		
      case X86_64_RELOC_SIGNED_4:		
      case X86_64_RELOC_TLV:
         throw error("unsupported relocation type");

      default:
         throw error("bad relocation type");
      }
   }

   template <Bits bits, typename T>
   RelocBlobT<bits, T>::RelocBlobT(const Image& img, const Location& loc, ParseEnv<bits>& env):
      RelocBlob<bits>(loc, env), data(img.at<T>(loc.offset)) {}

   template <Bits bits, typename T>
   void RelocBlobT<bits, T>::Emit(Image& img, std::size_t offset) const {
      img.at<T>(offset) = data;
   }
   
   template class SectionBlob<Bits::M32>;
   template class SectionBlob<Bits::M64>;
   
   template class LazySymbolPointer<Bits::M32>;
   template class LazySymbolPointer<Bits::M64>;

   template class DataBlob<Bits::M32>;
   template class DataBlob<Bits::M64>;

   template class Immediate<Bits::M32>;
   template class Immediate<Bits::M64>;

   template class RelocBlob<Bits::M32>;
   template class RelocBlob<Bits::M64>;
   
   template class RelocBlobT<Bits::M32, uint32_t>;
   template class RelocBlobT<Bits::M64, uint64_t>;
   template class RelocBlobT<Bits::M32, int32_t>;
   template class RelocBlobT<Bits::M64, int32_t>;

}
