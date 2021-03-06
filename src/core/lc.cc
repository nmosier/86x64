#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "lc.hh"
#include "segment.hh"
#include "image.hh"
#include "dyldinfo.hh"
#include "linkedit.hh"
#include "symtab.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob
#include "archive.hh" // Archive::Insert

namespace MachO {

   template <Bits bits>
   LoadCommand<bits> *LoadCommand<bits>::Parse(const Image& img, std::size_t offset,
                                               ParseEnv<bits>& env)
   {
      load_command lc = img.at<load_command>(offset);

      switch (lc.cmd) {
      case LC_SEGMENT:
         if constexpr (bits == Bits::M32) {
               return Segment<Bits::M32>::Parse(img, offset, env);
            } else {
            throw("32-bit segment command in 64-bit binary");
         }
         
      case LC_SEGMENT_64:
         if constexpr (bits == Bits::M64) {
               return Segment<Bits::M64>::Parse(img, offset, env);
            } else {
            throw("64-bit segment command in 32-bit binary");
         }

      case LC_DYLD_INFO:
      case LC_DYLD_INFO_ONLY:
         return DyldInfo<bits>::Parse(img, offset, env);

      case LC_SYMTAB:
         return Symtab<bits>::Parse(img, offset, env);

      case LC_DYSYMTAB:
         return Dysymtab<bits>::Parse(img, offset, env);

      case LC_LOAD_DYLINKER:
         return DylinkerCommand<bits>::Parse(img, offset, env);

      case LC_UUID:
         return UUID<bits>::Parse(img, offset, env);

      case LC_BUILD_VERSION:
         return BuildVersion<bits>::Parse(img, offset, env);

      case LC_SOURCE_VERSION:
         return SourceVersion<bits>::Parse(img, offset, env);

      case LC_MAIN:
         return EntryPoint<bits>::Parse(img, offset, env);

      case LC_LOAD_DYLIB:
      case LC_ID_DYLIB:
         return DylibCommand<bits>::Parse(img, offset, env);

      case LC_DATA_IN_CODE:
      case LC_FUNCTION_STARTS:
      case LC_CODE_SIGNATURE:
         return LinkeditData<bits>::Parse(img, offset, env);
         
      default:
         throw error("load command 0x%x not supported", lc.cmd);
      }
      
      // TODO
   }


   template <Bits bits>
   DylinkerCommand<bits>::DylinkerCommand(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), dylinker(img.at<dylinker_command>(offset))
   {
      if (dylinker.name.offset > dylinker.cmdsize) {
         throw error("dylinker name starts past end of command");
      }
      const std::size_t maxstrlen = dylinker.cmdsize - dylinker.name.offset;
      const std::size_t slen = strnlen(&img.at<char>(offset + dylinker.name.offset), maxstrlen);
      const char *strbegin = &img.at<char>(offset + dylinker.name.offset);
      name = std::string(strbegin, strbegin + slen);
   }

   template <Bits bits>
   std::size_t DylinkerCommand<bits>::size() const {
      return align_up(sizeof(dylinker_command) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   BuildVersion<bits>::BuildVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), build_version(img.at<build_version_command>(offset))
   {
      for (int i = 0; i < build_version.ntools; ++i) {
         tools.emplace_back(img, offset + sizeof(build_version) + i * BuildToolVersion::size());
      }
   }

   template <Bits bits>
   std::size_t BuildVersion<bits>::size() const {
      return sizeof(build_version) + BuildToolVersion::size() * tools.size();
   }

   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), entry_point(img.at<entry_point_command>(offset))
   {
      // env.offset_resolver.resolve(entry_point.entryoff, &entry);
   }

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), dylib_cmd(img.at<dylib_command>(offset))
   {
      const std::size_t stroff = dylib_cmd.dylib.name.offset;
      if (stroff > dylib_cmd.cmdsize) {
         throw error("dynamic library name starts past end of dylib command");
      }
      const std::size_t strrem = dylib_cmd.cmdsize - stroff;
      const std::size_t len = strnlen(&img.at<char>(offset + stroff), strrem);
      name = std::string(&img.at<char>(offset + stroff), &img.at<char>(offset + stroff + len));

      if (dylib_cmd.cmd == LC_LOAD_DYLIB) {
         env.dylib_resolver.add(this);
      }
   }

   template <Bits bits>
   std::size_t DylibCommand<bits>::size() const {
      return align_up(sizeof(dylib_cmd) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   void DylinkerCommand<bits>::Build(BuildEnv<bits>& env) {
      dylinker.cmdsize = size();
      dylinker.name.offset = sizeof(dylinker);
   }

   template <Bits bits>
   void BuildVersion<bits>::Build(BuildEnv<bits>& env) {
      build_version.cmdsize = size();
      build_version.ntools = tools.size();
   }

   template <Bits bits>
   void EntryPoint<bits>::Build(BuildEnv<bits>& env) {
      entry_point.cmdsize = size();
      entry_point.entryoff = entry->loc.offset;
   }

   template <Bits bits>
   void DylinkerCommand<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dylinker_command>(offset) = dylinker;
      memcpy(&img.at<char>(offset + dylinker.name.offset), name.c_str(), name.size() + 1);
   }

   template <Bits bits>
   void UUID<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uuid_command>(offset) = uuid;
   }

   template <Bits bits>
   void BuildVersion<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<build_version_command>(offset) = build_version;
      offset += sizeof(build_version_command);
      for (const BuildToolVersion& tool : tools) {
         tool.Emit(img, offset);
         offset += tool.size();
      }
   }

   void BuildToolVersion::Emit(Image& img, std::size_t offset) const {
      img.at<build_tool_version>(offset) = tool;
   }

   template <Bits bits>
   void SourceVersion<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<source_version_command>(offset) = source_version;
   }

   template <Bits bits>
   void EntryPoint<bits>::Emit(Image& img, std::size_t offset) const {
      assert(strcmp(entry->segment->segment_command.segname, SEG_TEXT) == 0);
      entry_point_command entry_point = this->entry_point;
      entry_point.entryoff = entry->loc.vmaddr - entry->segment->segment_command.vmaddr;
      img.at<entry_point_command>(offset) = entry_point;
   }

   template <Bits bits>
   void DylibCommand<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dylib_command>(offset) = dylib_cmd;
      offset += sizeof(dylib_command);
      memcpy(&img.at<char>(offset), name.c_str(), name.size() + 1);
      offset += name.size() + 1;
   }

   template <Bits bits>
   LoadCommand<bits>::LoadCommand(const LoadCommand<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env) {
      env.add(&other, this);
   }
   
   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const EntryPoint<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      LoadCommand<bits>(other, env), entry_point(other.entry_point), entry(nullptr)
   {
      env.resolve(other.entry, &entry);
   }

   template <Bits bits>
   void EntryPoint<bits>::Parse1(const Image& img, ParseEnv<bits>& env) {
      const std::size_t vmaddr = env.archive.offset_to_vmaddr(entry_point.entryoff);
      entry = env.add_placeholder(vmaddr);
   }

#if 0
   template <Bits bits>
   void EntryPoint<bits>::Parse2(ParseEnv<bits>& env) {
      Location loc(entry_point.entryoff, 0);
      Placeholder<bits> *entry = Placeholder<bits>::Parse(loc, env);
      env.archive.insert(entry, loc, Relation::BEFORE);
      this->entry = entry;
   }
#endif

   template <Bits bits>
   void DylibCommand<bits>::Build(BuildEnv<bits>& env) {
      dylib_cmd.cmdsize = size();
      dylib_cmd.dylib.name.offset = sizeof(dylib_cmd);
   }

   template <Bits bits>
   void DylibCommand<bits>::AssignID(BuildEnv<bits>& env) {
      switch (dylib_cmd.cmd) {
      case LC_LOAD_DYLIB:
         id = env.dylib_counter();
         break;

      case LC_ID_DYLIB:
         id = 0;
         break;

      default: abort();
      }
   }

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(uint32_t cmd, const std::string& name, uint32_t timestamp,
                                    uint32_t current_version, uint32_t compatibility_version):
      name(name)
   {
      dylib_cmd.cmd = cmd;
      dylib_cmd.dylib.timestamp = timestamp;
      dylib_cmd.dylib.current_version = current_version;
      dylib_cmd.dylib.compatibility_version = compatibility_version;
   }

   template class LoadCommand<Bits::M32>;
   template class LoadCommand<Bits::M64>;

   template class DylibCommand<Bits::M32>;
   template class DylibCommand<Bits::M64>;
   
}
