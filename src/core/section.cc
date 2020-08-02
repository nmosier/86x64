#include <sstream>
#include <string>
#include <iterator>

#include "section.hh"
#include "parse.hh"
#include "build.hh"
#include "transform.hh"
#include "section_blob.hh" // LazySymbolPointer
#include "instruction.hh"
#include "stub_helper.hh"

namespace MachO {

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t<bits> sect = img.at<section_t<bits>>(offset);
      uint32_t flags = sect.flags;
      
      /* check whether contains instructions */
      std::vector<std::string> text_sectnames = {SECT_TEXT, SECT_STUBS, SECT_SYMBOL_STUB};
      for (const std::string& sectname : text_sectnames) {
         if (sectname == sect.sectname) {
            return new Section<bits>(img, offset, env, TextParser);
         }
      }

      if (std::string(sect.sectname) == SECT_STUB_HELPER) {
         return new Section<bits>(img, offset, env, StubHelperParser);
      }
      
      if (flags == S_LAZY_SYMBOL_POINTERS) {
         return new Section<bits>(img, offset, env, LazySymbolPointer<bits>::Parse);
      } else if (flags == S_NON_LAZY_SYMBOL_POINTERS) {
         return new Section<bits>(img, offset, env, NonLazySymbolPointer<bits>::Parse);
      } else if (flags == S_REGULAR || flags == S_CSTRING_LITERALS) {
         return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
      } else if ((flags & S_ZEROFILL)) {
         return new Section<bits>(img, offset, env, ZeroBlob<bits>::Parse);
      }

      throw error("bad section flags (section %s)", sect.sectname);
   }

   template <Bits bits>
   SectionBlob<bits> *Section<bits>::TextParser(const Image& img, const Location& loc,
                                                    ParseEnv<bits>& env) {
      /* check if data in code */
      if (env.data_in_code.contains(loc.offset)) {
         return DataBlob<bits>::Parse(img, loc, env);
      } else {
         return Instruction<bits>::Parse(img, loc, env);
      }
   }


   template <Bits bits>
   Section<bits>:: ~Section() {
      for (SectionBlob<bits> *elem : content) {
         delete elem;
      }
   }
   
   template <Bits bits>
   void Section<bits>::Parse1(const Image& img, ParseEnv<bits>& env) {
      env.current_section = this;
      
      const std::size_t begin = sect.offset;
      const std::size_t end = begin + sect.size;
      std::size_t it = begin;
      std::size_t vmaddr = sect.addr;
      while (it != end) {
         SectionBlob<bits> *elem = parser(img, Location(it, vmaddr), env);
         elem->iter = content.insert(content.end(), elem);
         it += elem->size();
         vmaddr += elem->size();
      }

      env.current_section = nullptr;
   }
   
   template <Bits bits>
   std::string Section<bits>::name() const {
      return std::string(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
   }

   template <Bits bits>
   void Section<bits>::Build(BuildEnv<bits>& env) {
      env.align(sect.align);
      loc(env.loc);

      for (SectionBlob<bits> *elem : content) {
         elem->Build(env);
      }

      sect.size = env.loc.offset - loc().offset;

      Location relloc;
      sect.nreloc = relocs.size();
      env.allocate(sect.nreloc * RelocationInfo<bits>::size(), relloc);
      sect.reloff = relloc.offset;
   }

   template <Bits bits>
   void Section<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<section_t<bits>>(offset) = sect;

      std::size_t sect_offset = sect.offset;
      
      for (const SectionBlob<bits> *elem : content) {
         if (elem->active) {
            elem->Emit(img, sect_offset);
            sect_offset += elem->size();
         }
      }

      for (const RelocationInfo<bits> *reloc : relocs) {
         reloc->Emit(img, sect_offset, loc());
         sect_offset += RelocationInfo<bits>::size();
      }
   }

   template <Bits bits>
   void Section<bits>::Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob) {
      SectionBlob<bits> *elem = dynamic_cast<SectionBlob<bits> *>(blob);
      if (elem == nullptr) {
         throw std::invalid_argument(std::string(__FUNCTION__) + ": blob is of incorrect type");
      }
      auto it = content.begin();
      std::advance(it, loc.index);
      content.insert(it, elem);
      // content.insert(content.begin() + loc.index, elem);
   }

   template <Bits bits>
   Section<bits>::Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      /* relocs(other.relocs.Transform(env)), */ id(other.id)
   {
      env.add(&other, this);
      env(other.sect, sect);
      env.resolve(other.segment, &segment);
      for (const auto elem : other.content) {
         auto new_blobs = elem->Transform(env);
         if (!new_blobs.empty()) {
            env.add(elem, new_blobs.front());
         }
         content.splice(content.end(), new_blobs);
      }
   }

   template <Bits bits>
   typename Section<bits>::Content::iterator Section<bits>::find(std::size_t vmaddr) {
      typename Content::iterator prev = content.end();
      for (typename Content::iterator it = content.begin();
           it != content.end() && (*it)->loc.vmaddr <= vmaddr;
           prev = it, ++it)
         {}
      return prev;
   }

   template <Bits bits>
   typename Section<bits>::Content::const_iterator Section<bits>::find(std::size_t vmaddr) const {
      typename Content::const_iterator prev = content.end();
      for (typename Content::const_iterator it = content.begin();
           it != content.end() && (*it)->loc.vmaddr <= vmaddr;
           prev = it, ++it)
         {}
      return prev;
   }

#if 0
   template <Bits bits>
   void Section<bits>::Parse2(const Image& img, ParseEnv<bits>& env) {
      /* handle unresolved vmaddrs */
      auto vmaddr_resolver = env.vmaddr_resolver; /* maintain local copy */
      for (auto vmaddr_todo_it = vmaddr_resolver.todo.lower_bound(sect.addr);
           vmaddr_todo_it != vmaddr_resolver.todo.end() &&
              vmaddr_todo_it->first < sect.addr + sect.size;
           ++vmaddr_todo_it)
         {
            /* find closest instruction in section content */
            auto old_inst_it = content.begin();
            while (old_inst_it != content.end() &&
                   (*old_inst_it)->loc.vmaddr < vmaddr_todo_it->first) {
               ++old_inst_it;
            }
            if (old_inst_it == content.begin()) {
               continue;
            }
            --old_inst_it;
            assert((*old_inst_it)->loc.vmaddr < vmaddr_todo_it->first);

            /* instruction replacement loop */
            Location old_loc = (*old_inst_it)->loc;
            Location new_loc = old_loc + (vmaddr_todo_it->first - old_loc.vmaddr);

            /* erase instruction */
            old_inst_it = content.erase(old_inst_it);
            assert(old_inst_it != content.end());
            
            /* populate with data */
            for (Location data_loc = old_loc; data_loc != new_loc; ++data_loc) {
               content.insert(old_inst_it, DataBlob<bits>::Parse(img, data_loc, env));
            }
            old_loc = (*old_inst_it)->loc;            
            
            while (old_loc != new_loc) {
               std::clog << "old_loc=" << old_loc << " new_loc=" << new_loc << std::endl;
               
               if (old_loc < new_loc) {
                  /* now just erase instruction */
                  old_inst_it = content.erase(old_inst_it);
                  old_loc = (*old_inst_it)->loc;            
               } else if (old_loc > new_loc) {
                  /* parse and insert new instruction */
                  SectionBlob<bits> *new_inst = TextParser(img, new_loc, env);
                  // Instruction<bits> *new_inst = Instruction<bits>::Parse(img, new_loc, env);
                  content.insert(old_inst_it, new_inst);
                  new_loc += new_inst->size();
               } else {
                  throw std::logic_error(std::string(__FUNCTION__) +
                                         ": locations not completely ordered");
               }
            }
         }
   }
#endif

   template <Bits bits>
   void Section<bits>::Parse2(ParseEnv<bits>& env) {
      auto content_it = content.begin();

      /* for each placeholder, find blob in this section to insert it before */
      for (auto placeholder_it = env.placeholders.lower_bound(sect.addr);
           placeholder_it != env.placeholders.end() &&
              placeholder_it->first < sect.addr + sect.size;
           ++placeholder_it)
         {
            /* find first section blob that has an address greater than or equal to this
             * placeholder */
            content_it = std::lower_bound(content_it, content.end(), placeholder_it->first,
                                          [] (const SectionBlob<bits> *blob, std::size_t vmaddr) {
                                             return blob->loc.vmaddr < vmaddr;
                                          });
            if (content_it == content.end()) {
               fprintf(stderr, "failed to insert placeholder at 0x%zx: past end of section\n",
                       placeholder_it->first);
               throw std::logic_error("plaecholder insertion error");
            }

            assert((*content_it)->loc.vmaddr == placeholder_it->first);
            
            placeholder_it->second->segment = env.current_segment;
            placeholder_it->second->section = this;
            
            content.insert(content_it, placeholder_it->second);
         }
   }

   template <Bits bits>
   void Section<bits>::insert(SectionBlob<bits> *blob, const Location& loc, Relation rel) {
      blob->section = this;
      std::size_t Location::*locptr;
      std::size_t sectloc;
      if (loc.offset) {
         locptr = &Location::offset;
         sectloc = sect.offset;
      } else if (loc.vmaddr) {
         locptr = &Location::vmaddr;
         sectloc = sect.addr;
      } else {
         throw std::invalid_argument("location offset and vmaddr are both 0");
      }

      if (loc.*locptr >= sectloc && loc.*locptr < sectloc + sect.size) {
         /* find blob with given offset/address */
         auto it = std::upper_bound(content.begin(), content.end(), loc.*locptr,
                                    [=] (std::size_t loc, SectionBlob<bits> *blob) {
                                       return loc < blob->loc.*locptr;
                                    });
         switch (rel) {
         case Relation::BEFORE:
            --it;
            break;
         case Relation::AFTER:
            break;
         }
         content.insert(it, blob);
      } else {
         throw std::invalid_argument("location not in section");
      }
   }

   template <Bits bits>
   Section<bits>::Section(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser):
      sect(img.at<section_t<bits>>(offset)), segment(env.current_segment), parser(parser)
   {
      /* section resolver */
      env.section_resolver.add(this);
      
      /* read reloaction entries */
      std::size_t reloff = sect.reloff;
      for (uint32_t i = 0; i < sect.nreloc; ++i, reloff += RelocationInfo<bits>::size()) {
         relocs.push_back(RelocationInfo<bits>::Parse(img, reloff, env, loc()));
      }
   }

   template <Bits bits>
   RelocationInfo<bits>::RelocationInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                        const Location& baseloc):
      info(img.at<relocation_info>(offset))
   {
      /* create reloc blob */
      const Location relocloc = baseloc + info.r_address;
      relocee = RelocBlob<bits>::Parse(img, relocloc, env, info.r_type);
      env.relocs.insert({relocloc.vmaddr, relocee});
   }

   template <Bits bits>
   void RelocationInfo<bits>::Emit(Image& img, std::size_t offset, const Location& baseloc) const {
      relocation_info info = this->info;
      info.r_address = relocee->loc.vmaddr - baseloc.vmaddr;
      img.at<relocation_info>(offset) = info;
   }

   template <Bits bits>
   bool Section<bits>::contains_offset(std::size_t offset) const {
      return offset >= sect.offset && offset < sect.offset + sect.size;
   }

   template <Bits bits>
   bool Section<bits>::contains_vmaddr(std::size_t vmaddr) const {
      return vmaddr >= sect.addr && vmaddr < sect.addr + sect.size;
   }

   template <Bits bits>
   void Section<bits>::AssignID(BuildEnv<bits>& env) {
      id = env.section_counter();
   }

   template <Bits bits>
   SectionBlob<bits> *Section<bits>::StubHelperParser(const Image& img, const Location& loc,
                                                      ParseEnv<bits>& env) {
      if (StubHelperBlob<bits>::can_parse(img, loc, env)) {
         return StubHelperBlob<bits>::Parse(img, loc, env);
      } else {
         return Instruction<bits>::Parse(img, loc, env);
      }
   }

   template class Section<Bits::M32>;
   template class Section<Bits::M64>;
   
}
