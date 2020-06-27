#pragma once

#include "resolve.hh"
#include "types.hh"
#include "util.hh"

namespace MachO {

   template <Bits b1, Bits b2 = opposite<b1>>
   class TransformEnv {
   public:
      template <template<Bits> typename T>
      void add(const T<b1> *key, const T<b2> *pointee) {
         resolver.add(key, pointee);
      }
      template <template<Bits> typename T>
      void resolve(const T<b1> *key, const T<b2> **pointer) {
         resolver.resolve(key, (const void **) pointer);
      }

      void operator()(const mach_header_t<b1>& h1, mach_header_t<b2>& h2) const {
         memset(&h2, 0, sizeof(h2));
         memcpy(&h2, &h1, std::min(sizeof(h1), sizeof(h2)));
      }

      void operator()(const segment_command_t<b1>& h1, segment_command_t<b2>& h2) const {
         h2.cmd = b2 == Bits::M32 ? LC_SEGMENT : LC_SEGMENT_64;
         h2.cmdsize = 0; /* build-time */
         memcpy(h2.segname, h1.segname, sizeof(h2.segname));
         h2.vmaddr = h1.vmaddr;
         h2.vmsize = h1.vmsize;
         h2.fileoff = h1.fileoff;
         h2.filesize = h1.filesize;
         h2.maxprot = h1.maxprot;
         h2.initprot = h1.initprot;
         h2.nsects = 0; /* build-time */
         h2.flags = h1.flags;
      }

      void operator()(const nlist_t<b1>& n1, nlist_t<b2>& n2) const {
         n2.n_un.n_strx = 0; /* build-time */
         n2.n_type = n1.n_type;
         n2.n_sect = 0; /* build-time */
         n2.n_desc = n1.n_desc;
         n2.n_value = 0; /* build-time */
      }

      void operator()(const section_t<b1>& s1, section_t<b2>& s2) const {
         memset(&s2, 0, sizeof(s2));
         memcpy(s2.sectname, s1.sectname, sizeof(s2.sectname));
         memcpy(s2.segname, s1.segname, sizeof(s2.segname));
         s2.addr = 0; /* build-time */
         s2.size = 0; /* build-time */
         s2.offset = 0; /* build-time */
         s2.align = s1.align;
         s2.reloff = s1.reloff;
         s2.nreloc = s1.nreloc;
         s2.flags = s1.flags;
         s2.reserved1 = s1.reserved1;
         s2.reserved2 = s1.reserved2;
      }
      
   private:
      Resolver<const void *, void> resolver;
   };
   
}
