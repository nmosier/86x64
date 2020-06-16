#include <vector>

extern "C" {
#include <xed-interface.h>
}

#include <mach-o/loader.h>

#include "image.hh"
#include "macho-fwd.hh"
#include "util.hh"

namespace MachO {

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;

      virtual ~Section() {}
      
      static std::size_t size() { return sizeof(section_t); }
      static Section<bits> *Parse(const Image& img, std::size_t offset);

   protected:
      Section(const Image& img, std::size_t offset): sect(img.at<section_t>(offset)) {}
      virtual void Parse2(const Image& img, Archive<bits>&& archive) {}
   };

   template <Bits bits>
   class TextSection: public Section<bits> {
   public:
      using SectionBlobs = std::vector<SectionBlob<bits> *>;
      
      SectionBlobs blobs;
      
      template <typename... Args>
      static TextSection<bits> *Parse(Args&&... args) { return new TextSection(args...); }
      
   private:
      TextSection(const Image& img, std::size_t offset): Section<bits>(img, offset) {}
      virtual void Parse2(const Image& img, Archive<bits>&& archive);
   };

   template <Bits bits>
   class SectionBlob {
   public:
      static SectionBlob<bits> *Parse(const Image& img, std::size_t offset,
                                      Archive<bits>&& archive);
   protected:
      SectionBlob() {}
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      std::vector<uint8_t> data;

      template <typename... Args>
      static DataBlob<bits> *Parse(Args&&... args) { return new DataBlob(args...); }

   private:
      DataBlob(const Image& img, std::size_t offset, std::size_t size):
         data(&img.at<uint8_t>(offset), &img.at<uint8_t>(offset + size)) {}
   };
   
   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t dstate;
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      
      std::size_t size() const { return instbuf.size(); }
      
      template <typename... Args>
      static Instruction<bits> *Parse(Args&&... args) { return new Instruction(args...); }
      
   private:
      Instruction(const Image& img, std::size_t offset);
   };   

   template <Bits bits>
   class DataSection: public Section<bits> {
   public:
      std::vector<uint8_t> data;

      template <typename... Args>
      static DataSection<bits> *Parse(Args&&... args) { return new DataSection(args...); }
      
   private:
      DataSection(const Image& img, std::size_t offset):
         Section<bits>(img, offset), data(&img.at<uint8_t>(this->sect.offset),
                                           &img.at<uint8_t>(this->sect.offset + this->sect.size))
      {}      
   };

   template <Bits bits>
   class SymbolPointerSection: public Section<bits> {
   public:
      using pointer_t = select_type<bits, uint32_t, uint64_t>;

      std::vector<pointer_t> pointers;

      template <typename... Args>
      static SymbolPointerSection<bits> *Parse(Args&&... args)
      { return new SymbolPointerSection(args...); }
      
   private:
      SymbolPointerSection(const Image& img, std::size_t offset);
   };   
   
};
