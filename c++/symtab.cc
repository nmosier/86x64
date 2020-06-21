#include "symtab.hh"

namespace MachO {

   template <Bits bits>
   Symtab<bits>::Symtab(const Image& img, std::size_t offset):
      symtab(img.at<symtab_command>(offset)) {
      /* construct strings */
      const std::size_t strbegin = symtab.stroff;
      const std::size_t strend = strbegin + symtab.strsize;
      std::unordered_map<std::size_t, std::string *> off2str = {{0, nullptr}};
      for (std::size_t strit = strbegin + strnlen(&img.at<char>(strbegin), strend - strbegin) + 1;
           strit < strend; ) {
         const std::size_t strrem = strend - strit;
         std::size_t len = strnlen(&img.at<char>(strit), strrem);
         if (len == strrem) {
            throw error("string '%*s...' runs past end of string table",
                           strrem, &img.at<char>(strit));
         } else if (len > 0) {
            std::string *s = new std::string(&img.at<char>(strit));
            strs.push_back(s);
            off2str[strit - strbegin] = s;
            strit += len;
         }
         ++strit; /* skip null byte */
      }

      /* construct symbols */
      for (uint32_t i = 0; i < symtab.nsyms; ++i) {
         syms.push_back(Nlist<bits>::Parse(img, symtab.symoff + i * Nlist<bits>::size(), off2str));
      }

   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Image& img, std::size_t offset,
                      const std::unordered_map<std::size_t, std::string *>& off2str) {
      nlist = img.at<nlist_t>(offset);
      if (off2str.find(nlist.n_un.n_strx) == off2str.end()) {
         throw error("nlist offset 0x%x does not point to beginning of string", nlist.n_un.n_strx);
      }
      string = off2str.at(nlist.n_un.n_strx);
   }

   template <Bits bits>
   Dysymtab<bits>::Dysymtab(const Image& img, std::size_t offset):
      dysymtab(img.at<dysymtab_command>(offset)),
      indirectsyms(std::vector<uint32_t>(&img.at<uint32_t>(dysymtab.indirectsymoff),
                                         &img.at<uint32_t>(dysymtab.indirectsymoff) +
                                         dysymtab.nindirectsyms)) {}

   template <Bits bits>
   void Symtab<bits>::Build(BuildEnv<bits>& env) {
      /* allocate space for symbol table */
      symtab.nsyms = syms.size();
      symtab.symoff = env.allocate(Nlist<bits>::size() * symtab.nsyms);
      
      symtab.strsize = 0;
      for (const std::string *str : strs) {
         symtab.strsize += str->size();
      }
      symtab.stroff = env.allocate(symtab.strsize);

      for (Nlist<bits> *sym : syms) {
         (void) sym;
         // TODO
      }
   }
   
   template class Symtab<Bits::M32>;
   template class Symtab<Bits::M64>;
   
   template class Nlist<Bits::M32>;
   template class Nlist<Bits::M64>;

   template class Dysymtab<Bits::M32>;
   template class Dysymtab<Bits::M64>;
   
}
