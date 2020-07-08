#include <sstream>
#include <stdexcept>

#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"
#include "types.hh"

namespace MachO {

#if 0
   uint32_t lc_build_order[] =
      {LC_SEGMENT,
       LC_SEGMENT_64,
       LC_DYLD_INFO,
       LC_DYLD_INFO_ONLY,
       LC_DYSYMTAB,
       LC_SYMTAB,
      };
#endif
   // build order -- do later
   // rather, need build regions. e.g. 
   
   template <Bits bits>
   Archive<bits>::Archive(const Image& img, std::size_t offset):
      header(img.at<mach_header_t<bits>>(offset))
   {
      ParseEnv<bits> env(*this);
      offset += sizeof(header);
      for (int i = 0; i < header.ncmds; ++i) {
         LoadCommand<bits> *cmd = LoadCommand<bits>::Parse(img, offset, env);
         load_commands.push_back(cmd);
         offset += cmd->size();
      }

      for (LoadCommand<bits> *cmd : load_commands) {
         cmd->Parse1(img, env);
      }

      for (LoadCommand<bits> *cmd : load_commands) {
         cmd->Parse2(env);
      }
   }

   template <Bits bits>
   Archive<bits>::~Archive() {
      for (LoadCommand<bits> *lc : load_commands) {
         delete lc;
      }
   }

   template <Bits bits>
   Segment<bits> *Archive<bits>::segment(const std::string& name) {
      for (Segment<bits> *seg : segments()) {
         if (name == seg->name()) {
            return seg;
         }
      }
      return nullptr;
   }

   template <Bits bits>
   std::size_t Archive<bits>::Build(std::size_t offset) {
      BuildEnv<bits> env(this, Location(offset, vmaddr));
      
      env.allocate(sizeof(header));
      
      /* count number of load commands */
      header.ncmds = load_commands.size();

      /* compute size of commands */
      header.sizeofcmds = 0;
      for (LoadCommand<bits> *lc : load_commands) {
         header.sizeofcmds += lc->size();
      }

      env.allocate(header.sizeofcmds);

      /* assign IDs */
      for (LoadCommand<bits> *lc : load_commands) {
         lc->AssignID(env);
      }
      
      /* build each command */
      for (LoadCommand<bits> *lc : load_commands) {
         lc->Build(env);
      }

      total_size = env.loc.offset - offset;
      return total_size;
   }

   template <Bits bits>
   void Archive<bits>::Emit(Image& img) const {
      /* emit header */
      img.at<mach_header_t<bits>>(0) = header;
      
      /* emit load commands */
      std::size_t offset = sizeof(header);
      for (LoadCommand<bits> *lc : load_commands) {
         lc->Emit(img, offset);
         offset += lc->size();
      }
   }

   template <Bits bits>
   Archive<bits>::Archive(const Archive<opposite<bits>>& other, TransformEnv<opposite<bits>>& env)
   {
      env(other.header, header);
      for (const auto lc : other.load_commands) {
         load_commands.push_back(lc->Transform(env));
      }
   }

   template <Bits bits>
   void Archive<bits>::insert(SectionBlob<bits> *blob, const Location& loc, Relation rel) {
      macho_addr_t<bits> segment_command_t<bits>::*segloc;
      macho_addr_t<bits> segment_command_t<bits>::*segsize;
      std::size_t locval;
      if (loc.offset) {
         segloc = &segment_command_t<bits>::fileoff;
         segsize = &segment_command_t<bits>::filesize;
         locval = loc.offset;
      } else if (loc.vmaddr) {
         segloc = &segment_command_t<bits>::vmaddr;
         segsize = &segment_command_t<bits>::vmsize;
         locval = loc.vmaddr;
      } else {
         throw std::invalid_argument("location offset and vmaddr are both 0");
      }

      for (Segment<bits> *segment : segments()) {
         std::size_t segloc_ = segment->segment_command.*segloc;
         if (locval >= segloc_ && locval < segloc_ + segment->segment_command.*segsize) {
            segment->insert(blob, loc, rel);
            return;
         }
      }

      throw std::invalid_argument("location not in any segment");
   }

   template <Bits bits>
   std::size_t Archive<bits>::offset_to_vmaddr(std::size_t offset) const {
      for (Segment<bits> *segment : segments()) {
         if (offset >= segment->segment_command.fileoff &&
             offset < segment->segment_command.fileoff + segment->segment_command.filesize) {
            return segment->offset_to_vmaddr(offset);
         }
      }
      throw std::invalid_argument(std::string("offset" ) + std::to_string(offset) +
                                  " not in any segment");
   }

   template <Bits bits>
   std::optional<std::size_t> Archive<bits>::try_offset_to_vmaddr(std::size_t offset) const {
      for (Segment<bits> *segment : segments()) {
         if (segment->contains_offset(offset)) {
            return segment->try_offset_to_vmaddr(offset);
         }
      }
      return std::nullopt;
   }

   AbstractArchive *AbstractArchive::Parse(const Image& img, std::size_t offset) {
      uint32_t magic = img.at<uint32_t>(offset);
      switch (magic) {
      case MH_CIGAM:
      case MH_CIGAM_64:
         throw std::invalid_argument("archive has opposite endianness");

      case MH_MAGIC:
         return Archive<Bits::M32>::Parse(img, offset);
         
      case MH_MAGIC_64:
         return Archive<Bits::M64>::Parse(img, offset);

      default:
         {
            std::stringstream ss;
            ss << "invalid magic number 0x" << std::hex << magic;
            throw std::invalid_argument(ss.str());
         }
      }
   }

   template <Bits bits>
   void Archive<bits>::remove_commands(uint32_t cmd) {
      for (auto it = load_commands.begin(); it != load_commands.end(); ++it) {
         if ((*it)->cmd() == cmd) {
            it = load_commands.erase(it);
         }
      }
   }
   
   template class Archive<Bits::M32>;
   template class Archive<Bits::M64>;

}
