#pragma once

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "macho-fwd.hh"

namespace MachO {

   template <Bits bits>
   using mach_header_t = select_type<bits, mach_header, mach_header_64>;

   template <Bits bits>
   using segment_command_t = select_type<bits, segment_command, segment_command_64>;

   template <Bits bits>
   using nlist_t = select_type<bits, struct nlist, struct nlist_64>;

   template <Bits bits>
   using section_t = select_type<bits, struct section, struct section_64>;
   
}
