#include "segment.hh"
#include "util.hh"
#include "macho.hh"

namespace MachO {

   template <Bits bits>
   Segment<bits> *Segment<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      const char *segname = img.at<segment_command_t>(offset).segname;

      if (strcmp(segname, SEG_LINKEDIT) == 0) {
         return Segment_LINKEDIT<bits>::Parse(img, offset, env);
      } else {
         return new Segment(img, offset, env);
      }
   }

   
   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      segment_command = img.at<segment_command_t>(offset);

      env.current_segment = this;
      
      offset += sizeof(segment_command_t);
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
      segment_command.filesize = env.loc.offset - segment_command.fileoff;
      segment_command.vmsize = align_up<size_t>(env.loc.vmaddr - segment_command.vmaddr, PAGESIZE);
   }

   template <Bits bits>
   void Segment_LINKEDIT<bits>::Build(BuildEnv<bits>& env) {
      this->segment_command.fileoff = env.loc.offset = align_up(env.loc.offset, PAGESIZE);
      this->segment_command.vmaddr = env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      
      linkedits = env.archive.template subcommands<LinkeditCommand<bits>>();
      for (LinkeditCommand<bits> *linkedit : linkedits) {
         linkedit->Build_LINKEDIT(env);
      }
      
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      
      this->segment_command.filesize = env.loc.offset - this->segment_command.fileoff;
      this->segment_command.vmsize = align_up<std::size_t>(this->segment_command.filesize, PAGESIZE);
   }

   template <Bits bits>
   void Segment<bits>::Build_PAGEZERO(BuildEnv<bits>& env) {
      assert(env.loc.vmaddr == 0);
      segment_command.vmaddr = 0;
      segment_command.vmsize = vmaddr_start<bits>;
      segment_command.fileoff = 0;
      segment_command.filesize = 0;
      env.loc.vmaddr += vmaddr_start<bits>;
   }

   template <Bits bits>
   void Segment<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<segment_command_t>(offset) = segment_command;
      offset += sizeof(segment_command_t);
      
      for (const Section<bits> *sect : sections) {
         sect->Emit(img, offset);
         offset += sect->size();
      }

      fprintf(stderr, "[EMIT] segment={name=%s,fileoff=0x%zx,filesize=0x%zx,vmaddr=0x%zx,vmsize=0x%zx}\n", segment_command.segname, (std::size_t) segment_command.fileoff, (size_t) segment_command.filesize, (size_t) segment_command.vmaddr, (size_t) segment_command.vmsize);
   }

   template class Segment<Bits::M32>;
   template class Segment<Bits::M64>;

}
