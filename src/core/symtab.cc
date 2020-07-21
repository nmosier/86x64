#include <set>
#include <mach-o/stab.h>

#include "symtab.hh"
#include "segment.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob
#include "archive.hh" // Archive

namespace MachO {

   template <Bits bits>
   Symtab<bits>::Symtab(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), symtab(img.at<symtab_command>(offset))
   {
      /* construct strings */
      const std::size_t strbegin = symtab.stroff;
      const std::size_t strend = strbegin + symtab.strsize;
      std::unordered_map<std::size_t, String<bits> *> off2str; // = {{0, nullptr}};
      for (std::size_t strit = strbegin /* + strnlen(&img.at<char>(strbegin), strend - strbegin) + 1 */;
           strit < strend; ) {
         const std::size_t strrem = strend - strit;

         String<bits> *str = String<bits>::Parse(img, strit, strrem);
         strs.push_back(str);
         off2str[strit - strbegin] = str;
         strit += str->size(); // NOTE: includes null byte
      }

      /* construct symbols */
      for (uint32_t i = 0; i < symtab.nsyms; ++i) {
         syms.insert(Nlist<bits>::Parse(img, symtab.symoff + i * Nlist<bits>::size(), env,
                                        off2str));
      }

   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                      const std::unordered_map<std::size_t, String<bits> *>& off2str):
      value(nullptr)
   {
      nlist = img.at<nlist_t<bits>>(offset);
      if (off2str.find(nlist.n_un.n_strx) == off2str.end()) {
         throw error("nlist offset 0x%x does not point to beginning of string", nlist.n_un.n_strx);
      }
      string = off2str.at(nlist.n_un.n_strx);
      if (string->str == MH_EXECUTE_HEADER) {
         // do nothing
      } else {
         value = env.add_placeholder(nlist.n_value);
      }

      /* resolve section */
      if (type() == Type::SECT) {
         if (nlist.n_sect == NO_SECT) {
            throw error("symbol `%s' type is SECT but section number is NO_SECT (0)",
                        string->str.c_str());
         }
         env.section_resolver.resolve(nlist.n_sect, &section);
      }
   }

   template <Bits bits>
   Dysymtab<bits>::Dysymtab(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dysymtab(img.at<dysymtab_command>(offset)),
      indirectsyms(std::vector<uint32_t>(&img.at<uint32_t>(dysymtab.indirectsymoff),
                                         &img.at<uint32_t>(dysymtab.indirectsymoff) +
                                         dysymtab.nindirectsyms)) {}

   template <Bits bits>
   void Symtab<bits>::Build_LINKEDIT_symtab(BuildEnv<bits>& env) {
      symtab.nsyms = syms.size();
      symtab.symoff = env.allocate(Nlist<bits>::size() * symtab.nsyms);
      
      for (auto sym : syms) {
         sym->Build(env);
      }
   }

   template <Bits bits>
   void Symtab<bits>::Build_LINKEDIT_strtab(BuildEnv<bits>& env) {
      BuildEnv<bits> strtab_env(env.archive, Location(0, 0));
      for (String<bits> *str : strs) {
         str->Build(strtab_env);
      }
      
      symtab.strsize = strtab_env.loc.offset;
      symtab.stroff = env.allocate(symtab.strsize);
   }
   
   template <Bits bits>
   String<bits>::String(const Image& img, std::size_t offset, std::size_t maxlen) {
      std::size_t slen = strnlen(&img.at<char>(offset), maxlen);
      str = std::string(&img.at<char>(offset), slen);
   }

   template <Bits bits>
   void String<bits>::Build(BuildEnv<bits>& env) {
      offset = env.allocate(size());
   }

   template <Bits bits>
   void Dysymtab<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      /* compute counts of symbol types (local, ext, undef) */
      std::size_t symindex = 0;
      auto symtab = env.archive->template subcommand<Symtab>();
      dysymtab.nlocalsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::LOCAL;
         });
      dysymtab.ilocalsym = symindex;
      symindex += dysymtab.nlocalsym;

      dysymtab.nextdefsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::EXT;
         });
      dysymtab.iextdefsym = symindex;
      symindex += dysymtab.nextdefsym;

      dysymtab.nundefsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::UNDEF; 
         });
      dysymtab.iundefsym = symindex;
      symindex += dysymtab.nundefsym;
      
      dysymtab.indirectsymoff = env.allocate(align<bits>(sizeof(uint32_t) * indirectsyms.size()));
      dysymtab.nindirectsyms = indirectsyms.size();
   }

   template <Bits bits>
   std::size_t Dysymtab<bits>::content_size() const {
      return align<bits>(indirectsyms.size() * sizeof(uint32_t));
   }

   template <Bits bits>
   void Symtab<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<symtab_command>(offset) = symtab;

      std::size_t symoff = symtab.symoff;
      for (const Nlist<bits> *sym : syms) {
         sym->Emit(img, symoff);
         symoff += sym->size();
      }

      std::size_t stroff = symtab.stroff;
      for (const String<bits> *str : strs) {
         str->Emit(img, stroff);
         stroff += str->size();
      }
   }

   template <Bits bits>
   void Nlist<bits>::Build(BuildEnv<bits>& env) {
      /* get text address */
      if (string->str == MH_EXECUTE_HEADER) {
         nlist.n_value = env.archive->segment(SEG_TEXT)->loc().vmaddr;
      }

      /* get section ID */
      nlist.n_sect = section ? section->id : NO_SECT;
   }

   template <Bits bits>
   void Nlist<bits>::Emit(Image& img, std::size_t offset) const {
      nlist_t<bits> nlist = this->nlist;
      nlist.n_un.n_strx = string->offset;

      if (value) {
         nlist.n_value = value->loc.vmaddr;
      }

      img.at<nlist_t<bits>>(offset) = nlist;
   }
   
   template <Bits bits>
   void String<bits>::Emit(Image& img, std::size_t offset) const {
      img.copy(offset, str.c_str(), str.size() + 1);
      // memcpy(&img.at<char>(offset), str.c_str(), str.size() + 1);
   }

   template <Bits bits>
   void Dysymtab<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dysymtab_command>(offset) = dysymtab;
      img.copy(dysymtab.indirectsymoff, indirectsyms.begin(), indirectsyms.size());
   }

   template <Bits bits>
   std::size_t Symtab<bits>::content_size() const {
      std::size_t size = Nlist<bits>::size() * syms.size();
      for (const String<bits> *str : strs) {
         size += str->size();
      }
      return align<bits>(size);
   }

   template <Bits bits>
   Symtab<bits>::Symtab(const Symtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      LinkeditCommand<bits>(other, env), symtab(other.symtab)
   {
      for (const auto sym : other.syms) {
         if (sym->string->str != Nlist<bits>::DYLD_PRIVATE || 1) {
            syms.insert(sym->Transform(env));
         }
      }

      for (const auto str : other.strs) {
         if (str->str != Nlist<bits>::DYLD_PRIVATE || 1) {
            strs.push_back(str->Transform(env));
         }
      }
   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Nlist<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      string(nullptr), value(nullptr)
   {
      env(other.nlist, nlist);
      env.resolve(other.string, &string);

      if (other.string->str == MH_EXECUTE_HEADER) {
         // value remains null
      } else {
         env.resolve(other.value, &value);
      }

      /* resolve section */
      env.resolve(other.section, &section);
   }
   
   template <Bits bits>
   String<bits>::String(const String<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      str(other.str), offset(0)
   {
      env.add(&other, this);
   }

   template <Bits bits>
   void Symtab<bits>::remove(const std::string& name) {
      /* find symbol to be removed */
      for (auto it = syms.begin(); it != syms.end(); ++it) {
         if ((*it)->string->str == name) {
            syms.erase(it);
         }
      }
      strs.remove_if([&] (auto str) { return name == str->str; });
   }

   template <Bits bits>
   typename Nlist<bits>::Kind Nlist<bits>::kind() const {
      if ((nlist.n_type & N_EXT) == 0) {
         return Kind::LOCAL;
      } else if ((nlist.n_type & N_TYPE) == N_UNDF) {
         return Kind::UNDEF;
      } else {
         return Kind::EXT;
      }
   }
   
   template <Bits bits>
   bool Symtab<bits>::NlistCompare::operator()(const Nlist<bits> *lhs,
                                               const Nlist<bits> *rhs) const {
      return (int) lhs->kind() < (int) rhs->kind();
   }

   template <Bits bits>
   void Symtab<bits>::print(std::ostream& os) const {
      os << "nsyms=" << syms.size() << std::endl;
      
      /* print column names */
      os << "value type external desc section string" << std::endl;
      
      for (auto sym : syms) {
         sym->print(os);
         os << std::endl;
      }
   }

   template <Bits bits>
   void Nlist<bits>::print(std::ostream& os) const {
      /* value */
      os << (value ? value->loc.vmaddr : nlist.n_value) << " ";

      /* type */
      const std::unordered_map<uint8_t, const char *> type2str =
         {{N_UNDF, "UNDF"},
          {N_ABS,  "ABS"},
          {N_SECT, "SECT"},
          {N_PBUD, "PBUD"},
          {N_INDR, "INDR"}};
      const auto type_it = type2str.find(nlist.n_type & N_TYPE);
      os << (type_it != type2str.end() ? type_it->second : "?") << " ";

      /* external? */
      os << (nlist.n_type & N_EXT ? 'x' : '-') << " ";

      /* stab info? */
      if ((nlist.n_type & N_EXT)) {
         os << nlist.n_desc;
      } else {
         os << "-";
      }
      os << " ";

      /* section */
      switch (nlist.n_type & N_TYPE) {
      case N_UNDF:
      case N_ABS:
      case N_PBUD:
      case N_INDR:
         os << "-";
         break;
      case N_SECT:
         os << section->segment->name() << "," << section->name() << " ";
         break;
      default:
         os << "-";
      }
      os << " ";
      
      os << string->str;
   }
   
   template class Symtab<Bits::M32>;
   template class Symtab<Bits::M64>;
   
   template class Nlist<Bits::M32>;
   template class Nlist<Bits::M64>;

   template class Dysymtab<Bits::M32>;
   template class Dysymtab<Bits::M64>;
   
}
