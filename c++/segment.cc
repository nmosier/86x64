#include <sstream>

#include "segment.hh"
#include "util.hh"
#include "transform.hh"
#include "archive.hh"
#include "types.hh"
#include "section_blob.hh"
#include "dyldinfo.hh"
#include "linkedit.hh"
#include "data_in_code.hh"
#include "symtab.hh"


namespace MachO {

   template <Bits bits>
   Segment<bits> *Segment<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      const char *segname = img.at<segment_command_t<bits>>(offset).segname;
      
      if (strcmp(segname, SEG_LINKEDIT) == 0) {
         return Segment_LINKEDIT<bits>::Parse(img, offset, env);
      } else {
         return new Segment(img, offset, env);
      }
   }
   
   
   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env)
   {
      segment_command = img.at<segment_command_t<bits>>(offset);

      env.current_segment = this;
      
      offset += sizeof(segment_command_t<bits>);
      for (int i = 0; i < segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset, env);
         sections.push_back(section);
         offset += section->size();
      }
      
      env.current_segment = nullptr;
   }

   template <Bits bits>
   Segment<bits>::~Segment() {
      for (Section<bits> *ptr : sections) {
         delete ptr;
      }
   }   

   template <Bits bits>
   std::size_t Segment<bits>::size() const {
      return sizeof(segment_command) + sections.size() * Section<bits>::size();
   }

   template <Bits bits>
   Section<bits> *Segment<bits>::section(const std::string& name) {
      for (Section<bits> *section : sections) {
         if (section->name() == name) {
            return section;
         }
      }
      return nullptr;
   }

   template <Bits bits>
   std::string Segment<bits>::name() const {
      return std::string(segment_command.segname, strnlen(segment_command.segname,
                                                          sizeof(segment_command.segname)));
   }

   template <Bits bits>
   void Segment<bits>::Build(BuildEnv<bits>& env) {
      assert(env.loc.vmaddr % PAGESIZE == 0);
      segment_command.nsects = sections.size();
      segment_command.cmdsize =
         sizeof(segment_command) + Section<bits>::size() * segment_command.nsects;
      
      if (strcmp(segment_command.segname, SEG_PAGEZERO) == 0) {
         Build_PAGEZERO(env);
         return;
      }

      /* set segment start location */
      if (strcmp(segment_command.segname, SEG_TEXT) == 0) {
         env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
         segment_command.fileoff = align_down(env.loc.offset, PAGESIZE);
         segment_command.vmaddr = env.loc.vmaddr;
         env.loc.vmaddr += env.loc.offset % PAGESIZE;
      } else {
         env.newsegment();
         segment_command.fileoff = env.loc.offset;
         segment_command.vmaddr = env.loc.vmaddr;
      }
      
      for (Section<bits> *sect : sections) {
         sect->Build(env);
      }
      
      /* post-conditions for vmaddr */
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      env.loc.offset = align_up(env.loc.offset, PAGESIZE); /* experimental! */
      
      segment_command.filesize = env.loc.offset - segment_command.fileoff;
      segment_command.vmsize = align_up<size_t>(env.loc.vmaddr - segment_command.vmaddr, PAGESIZE);
   }

   template <Bits bits>
   void Segment_LINKEDIT<bits>::Build(BuildEnv<bits>& env) {
      this->segment_command.cmdsize = sizeof(segment_command_t<bits>);
      this->segment_command.nsects = 0;
      this->segment_command.fileoff = env.loc.offset = align_up(env.loc.offset, PAGESIZE);
      this->segment_command.vmaddr = env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      
      // linkedits = env.archive->template subcommands<LinkeditCommand<bits>>();

      /* LC_DYLD_INFO[_ONLY] */
      auto dyld_info = env.archive->template subcommand<DyldInfo<bits>>();
      if (dyld_info) { dyld_info->Build_LINKEDIT(env); }

      /* LC_FUNCTION_STARTS */
      auto function_starts = env.archive->template subcommand<FunctionStarts<bits>>();
      if (function_starts) { function_starts->Build_LINKEDIT(env); }

      /* LC_DATA_IN_CODE */
      auto data_in_code = env.archive->template subcommand<DataInCode<bits>>();
      if (data_in_code) { data_in_code->Build_LINKEDIT(env); }

      /* LC_SYMTAB: symbol table */
      auto symtab = env.archive->template subcommand<Symtab<bits>>();
      if (symtab) { symtab->Build_LINKEDIT_symtab(env); }

      /* LC_DYSYMTAB */
      auto dysymtab = env.archive->template subcommand<Dysymtab<bits>>();
      if (dysymtab) { dysymtab->Build_LINKEDIT(env); }

      /* LC_SYMTAB: string table */
      if (symtab) { symtab->Build_LINKEDIT_strtab(env); }

      /* LC_CODE_SIGNATURE */
      auto code_signature = env.archive->template subcommand<CodeSignature<bits>>();
      if (code_signature) { code_signature->Build_LINKEDIT(env); }

      
      for (LinkeditCommand<bits> *linkedit : linkedits) {
         linkedit->Build_LINKEDIT(env);
      }
      
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      
      this->segment_command.filesize = env.loc.offset - this->segment_command.fileoff;
      this->segment_command.vmsize = align_up<std::size_t>(this->segment_command.filesize,
                                                           PAGESIZE);
   }

   template <Bits bits>
   void Segment<bits>::Build_PAGEZERO(BuildEnv<bits>& env) {
      segment_command.vmaddr = 0;
      segment_command.vmsize = env.loc.vmaddr;
      segment_command.fileoff = 0;
      segment_command.filesize = 0;
   }

   template <Bits bits>
   void Segment<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<segment_command_t<bits>>(offset) = segment_command;
      offset += sizeof(segment_command_t<bits>);
      
      for (const Section<bits> *sect : sections) {
         sect->Emit(img, offset);
         offset += sect->size();
      }

      fprintf(stderr, "[EMIT] segment={name=%s,fileoff=0x%zx,filesize=0x%zx,vmaddr=0x%zx,vmsize=0x%zx}\n", segment_command.segname, (std::size_t) segment_command.fileoff, (size_t) segment_command.filesize, (size_t) segment_command.vmaddr, (size_t) segment_command.vmsize);
   }

   template <Bits bits>
   Segment<bits>::Segment(const Segment<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      LoadCommand<bits>(other, env), id(0)
   {
      env(other.segment_command, segment_command);

      for (const auto other_section : other.sections) {
         sections.push_back(other_section->Transform(env));
      }
   }

   template <Bits bits>
   void Segment<bits>::insert(SectionBlob<bits> *blob, const Location& loc, Relation rel) {
      std::size_t locval;
      if (loc.offset) {
         locval = loc.offset;
      } else if (loc.vmaddr) {
         locval = loc.vmaddr;
      } else {
         throw std::invalid_argument("vmaddr and offsets are both 0");
      }

      auto sectloc = [=] (const Section<bits> *section) -> std::size_t {
                        if (loc.offset) {
                           return section->sect.offset;
                        } else {
                           return section->sect.addr;
                        }

                     };
      
      for (Section<bits> *section : sections) {
         std::size_t start = sectloc(section);
         if (start <= locval && locval < start + section->sect.size) {
            blob->segment = this;
            section->insert(blob, loc, rel);
            return;
         }
      }

      throw std::invalid_argument("location not found");
   }

   template <Bits bits>
   std::size_t Segment<bits>::offset_to_vmaddr(std::size_t offset) const {
      for (Section<bits> *section : sections) {
         if (offset >= section->sect.offset &&
             offset < section->sect.offset + section->sect.size) {
            return offset - (std::size_t) section->sect.offset + (std::size_t) section->sect.addr;
         }
      }
      throw std::invalid_argument(std::string("offset ") + std::to_string(offset) +
                                  " not in any section");
   }

   template <Bits bits>
   std::optional<std::size_t> Segment<bits>::try_offset_to_vmaddr(std::size_t offset) const {
      for (Section<bits> *section : sections) {
         if (section->contains_offset(offset)) {
            return offset - section->sect.offset + section->sect.addr;
         }
      }
      return std::nullopt;
   }

   template <Bits bits>
   bool Segment<bits>::contains_vmaddr(std::size_t vmaddr) const {
      return vmaddr >= segment_command.vmaddr &&
         vmaddr < segment_command.vmaddr + segment_command.vmsize;
   }

   template <Bits bits>
   bool Segment<bits>::contains_offset(std::size_t offset) const {
      return offset >= segment_command.fileoff &&
         offset < segment_command.fileoff + segment_command.filesize;
   }

   template class Segment<Bits::M32>;
   template class Segment<Bits::M64>;

}
