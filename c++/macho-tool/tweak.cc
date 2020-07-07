#include <fcntl.h>
#include <iostream>

#include "../tweak.hh"
#include "tweak.hh"

int TweakCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'f': /* mach_header_t flags */
      parse_flags(optarg,
                  {MH_FLAG(MH_NOUNDEFS),
                   MH_FLAG(MH_INCRLINK),
                   MH_FLAG(MH_DYLDLINK),
                   MH_FLAG(MH_BINDATLOAD),
                   MH_FLAG(MH_PREBOUND),
                   MH_FLAG(MH_SPLIT_SEGS),
                   MH_FLAG(MH_LAZY_INIT),
                   MH_FLAG(MH_TWOLEVEL),
                   MH_FLAG(MH_FORCE_FLAT),
                   MH_FLAG(MH_NOMULTIDEFS),
                   MH_FLAG(MH_NOFIXPREBINDING),
                   MH_FLAG(MH_PREBINDABLE),
                   MH_FLAG(MH_ALLMODSBOUND),
                   MH_FLAG(MH_SUBSECTIONS_VIA_SYMBOLS),
                   MH_FLAG(MH_CANONICAL),
                   MH_FLAG(MH_WEAK_DEFINES),
                   MH_FLAG(MH_BINDS_TO_WEAK),
                   MH_FLAG(MH_ALLOW_STACK_EXECUTION),
                   MH_FLAG(MH_ROOT_SAFE),
                   MH_FLAG(MH_SETUID_SAFE),
                   MH_FLAG(MH_NO_REEXPORTED_DYLIBS),
                   MH_FLAG(MH_PIE),
                   MH_FLAG(MH_DEAD_STRIPPABLE_DYLIB),
                   MH_FLAG(MH_HAS_TLV_DESCRIPTORS),
                   MH_FLAG(MH_NO_HEAP_EXECUTION),
                   MH_FLAG(MH_APP_EXTENSION_SAFE),
                   MH_FLAG(MH_NLIST_OUTOFSYNC_WITH_DYLDINFO),
                   MH_FLAG(MH_SIM_SUPPORT),
                  },
                  mach_header_flags);
      return 1;

   case 't':
      {
         auto it = mach_header_filetype_map.find(optarg);
         if (it != mach_header_filetype_map.end()) {
            mach_header_filetype = it->second;
         } else {
            throw std::string("-t: unrecognized Mach-O archive filetype");
         }
      }
      return 1;
            
   case 'h':
      usage(std::cout);
      return 0;

   default: abort();
   }
}

int TweakCommand::work() {
   auto macho = MachO::Tweak::MachO::Parse(*img);
   auto archive = dynamic_cast<MachO::Tweak::Archive<MachO::Bits::M64> *>(macho);

   if (mach_header_filetype) {
      archive->header.filetype = *mach_header_filetype;
   }
      
   if (!mach_header_flags.empty()) {
      if (archive == nullptr) {
         log("--flags: modifying mach header flags  only supported for 64-bit archives");
         return -1;
      }

      for (auto pair : mach_header_flags) {
         if (pair.second) {
            /* add flag */
            archive->header.flags |= pair.first;
         } else {
            archive->header.flags &= ~pair.first;
         }
      }
   }

   return 0;
}

TweakCommand::TweakCommand(): InplaceCommand("tweak", O_RDWR) {}
