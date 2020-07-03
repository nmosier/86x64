#pragma once

#include <list>
#include <vector>
#include <sstream>

#include "util.hh"
#include "loc.hh"
#include "image.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class Section {
   public:
      using Content = std::list<SectionBlob<bits> *>;
      
      section_t<bits> sect;
      Content content;

      std::string name() const;
      Location loc() const { return Location(sect.offset, sect.addr); }
      void loc(const Location& loc) { sect.offset = loc.offset; sect.addr = loc.vmaddr; }

      virtual ~Section();
      
      static std::size_t size() { return sizeof(section_t<bits>); }

      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      void Parse1(const Image& img, ParseEnv<bits>& env);
      void Parse2(ParseEnv<bits>& env);

      virtual void Build(BuildEnv<bits>& env);
      std::size_t content_size() const;
      void Emit(Image& img, std::size_t offset) const;
      void Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob);

      virtual Section<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Section<opposite<bits>>(*this, env);
      }

      void insert(SectionBlob<bits> *blob, const Location& loc, Relation rel);

      template <template <Bits> class Blob>
      Blob<bits> *find_blob(std::size_t vmaddr) const {
         auto it = std::lower_bound(content.begin(), content.end(), vmaddr,
                                    [] (const SectionBlob<bits> *blob, std::size_t vmaddr) {
                                       return blob->loc.vmaddr < vmaddr;
                                    });
         if (it == content.end()) {
            std::stringstream ss;
            ss << "vmaddr " << std::hex << vmaddr << " not in section " << sect.sectname;
            throw std::invalid_argument(ss.str());
         } else if ((*it)->loc.vmaddr != vmaddr) {
            std::stringstream ss;
            ss << "vmaddr " << std::hex << vmaddr << " not aligned with blob start";
            throw std::invalid_argument(ss.str());
         } else {
            Blob<bits> *blob = nullptr;
            while (it != content.end() &&
                   (*it)->loc.vmaddr == vmaddr &&
                   (blob = dynamic_cast<Blob<bits> *>(*it)) == nullptr) {
               ++it;
            }
            
            if (blob == nullptr) {
               throw std::invalid_argument("blob at vmaddr is not of correct type");
            } else {
               return blob;
            }
         }
      }

      bool contains_vmaddr(std::size_t vmaddr) const;
      
      typename Content::iterator find(std::size_t vmaddr); /* inclusive greatest lower bound */
      typename Content::const_iterator find(std::size_t vmaddr) const;

   protected:
      typedef SectionBlob<bits> *(*Parser)(const Image&, const Location&, ParseEnv<bits>&);
      Parser parser;
      
      Section(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser):
         sect(img.at<section_t<bits>>(offset)), parser(parser) {}
      Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      static SectionBlob<bits> *TextParser(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env);

      template <Bits> friend class Section;
   };

}
