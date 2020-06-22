#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "lc.hh"
#include "segment.hh"
#include "image.hh"
#include "dyldinfo.hh"
#include "linkedit.hh"
#include "macho.hh"
#include "symtab.hh"

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
         return Symtab<bits>::Parse(img, offset);

      case LC_DYSYMTAB:
         return Dysymtab<bits>::Parse(img, offset);

      case LC_LOAD_DYLINKER:
         return DylinkerCommand<bits>::Parse(img, offset);

      case LC_UUID:
         return UUID<bits>::Parse(img, offset);

      case LC_BUILD_VERSION:
         return BuildVersion<bits>::Parse(img, offset);

      case LC_SOURCE_VERSION:
         return SourceVersion<bits>::Parse(img, offset);

      case LC_MAIN:
         return EntryPoint<bits>::Parse(img, offset, env);

      case LC_LOAD_DYLIB:
         return DylibCommand<bits>::Parse(img, offset, env);

      case LC_DATA_IN_CODE:
         fprintf(stderr, "warning: %s: data in code not yet supported\n", __FUNCTION__);
      case LC_FUNCTION_STARTS:
      case LC_CODE_SIGNATURE:
         return LinkeditData<bits>::Parse(img, offset);
         
      default:
         throw error("load command 0x%x not supported", lc.cmd);
      }
      
      // TODO
   }


   template <Bits bits>
   DylinkerCommand<bits>::DylinkerCommand(const Image& img, std::size_t offset):
      dylinker(img.at<dylinker_command>(offset)) {
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
   BuildVersion<bits>::BuildVersion(const Image& img, std::size_t offset):
      build_version(img.at<build_version_command>(offset)) {
      for (int i = 0; i < build_version.ntools; ++i) {
         tools.push_back(BuildToolVersion<bits>::Parse(img, offset +
                                                       sizeof(build_version) +
                                                       i * BuildToolVersion<bits>::size()));
      }
   }

   template <Bits bits>
   std::size_t BuildVersion<bits>::size() const {
      return sizeof(build_version) + BuildToolVersion<bits>::size() * tools.size();
   }

   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      entry_point(img.at<entry_point_command>(offset)) {
      env.offset_resolver.resolve(entry_point.entryoff, &entry);
   }

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      dylib_cmd(img.at<dylib_command>(offset))
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
      dylinker.name.offset = sizeof(dylinker);
   }

   template <Bits bits>
   void BuildVersion<bits>::Build(BuildEnv<bits>& env) {
      build_version.ntools = tools.size();
   }

   template <Bits bits>
   void EntryPoint<bits>::Build(BuildEnv<bits>& env) {
      entry_point.entryoff = entry->loc.offset;
   }

   template <Bits bits>
   void DylibCommand<bits>::Build(BuildEnv<bits>& env) {
      dylib_cmd.dylib.name.offset = sizeof(dylib_cmd);
      id = env.template counter<decltype(this)>();
      // id = env.register_dylib();
   }

   template class LoadCommand<Bits::M32>;
   template class LoadCommand<Bits::M64>;
   
}
