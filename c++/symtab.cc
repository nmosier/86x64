#include "symtab.hh"
#include "segment.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob

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
         syms.push_back(Nlist<bits>::Parse(img, symtab.symoff + i * Nlist<bits>::size(), env,
                                           off2str));
      }

   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                      const std::unordered_map<std::size_t, String<bits> *>& off2str):
      value(nullptr)
   {
      nlist = img.at<nlist_t<bits>>(offset);
      value = env.add_placeholder(nlist.n_value);
      if (off2str.find(nlist.n_un.n_strx) == off2str.end()) {
         throw error("nlist offset 0x%x does not point to beginning of string", nlist.n_un.n_strx);
      }
      string = off2str.at(nlist.n_un.n_strx);
   }

   template <Bits bits>
   Dysymtab<bits>::Dysymtab(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dysymtab(img.at<dysymtab_command>(offset)),
      indirectsyms(std::vector<uint32_t>(&img.at<uint32_t>(dysymtab.indirectsymoff),
                                         &img.at<uint32_t>(dysymtab.indirectsymoff) +
                                         dysymtab.nindirectsyms)) {}

   template <Bits bits>
   void Symtab<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      /* allocate space for symbol table */
      symtab.nsyms = syms.size();
      symtab.symoff = env.allocate(Nlist<bits>::size() * symtab.nsyms);
      
      symtab.strsize = 0;
      for (const String<bits> *str : strs) {
         symtab.strsize += str->size();
      }
      symtab.strsize = align<bits>(symtab.strsize);
      symtab.stroff = env.allocate(symtab.strsize);
      
      /* create build environment for string table */
      BuildEnv<bits> strtab_env(env.archive, Location(0, 0));
      for (String<bits> *str : strs) {
         str->Build(strtab_env);
      }

      /* build nlists */
      for (Nlist<bits> *sym : syms) {
         sym->Build();
      }
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
   void Nlist<bits>::Build() {
      nlist.n_un.n_strx = string->offset;
   }

   template <Bits bits>
   void Dysymtab<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
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
   void Nlist<bits>::Emit(Image& img, std::size_t offset) const {
      nlist_t<bits> nlist = this->nlist;
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
      // memcpy(&img.at<uint32_t>(dysymtab.indirectsymoff), &*indirectsyms.begin(),
      // indirectsyms.size() * sizeof(uint32_t));
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
         // DEBUG
         if (sym->string->str != "__dyld_private" || 1) {
            syms.push_back(sym->Transform(env));
         }
      }
      for (const auto str : other.strs) {
         if (str->str != "__dyld_private" || 1) {
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
      env.resolve(other.value, &value);
   }
   
   template <Bits bits>
   String<bits>::String(const String<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      str(other.str), offset(0)
   {
      env.add(&other, this);
   }

   template class Symtab<Bits::M32>;
   template class Symtab<Bits::M64>;
   
   template class Nlist<Bits::M32>;
   template class Nlist<Bits::M64>;

   template class Dysymtab<Bits::M32>;
   template class Dysymtab<Bits::M64>;
   
}
