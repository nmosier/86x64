#include <vector>
#include <list>
#include <unordered_map>

extern "C" {
#include <xed-interface.h>
}

#include <mach-o/loader.h>

#include "image.hh"
#include "util.hh"
#include "lc.hh"
#include "parse.hh"

namespace MachO {
   
   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using segment_command_t = select_type<bits, segment_command, segment_command_64>;
      using Sections = std::vector<Section<bits> *>;

      segment_command_t segment_command;
      Sections sections;

      virtual uint32_t cmd() const override { return segment_command.cmd; }
      virtual std::size_t size() const override;

      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   protected:
      Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };   

   template <Bits bits>
   class Section {
   public:
      using section_t = select_type<bits, section, section_64>;

      section_t sect;

      virtual ~Section() {}
      
      static std::size_t size() { return sizeof(section_t); }
      static Section<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);

   protected:
      Section(const Image& img, std::size_t offset): sect(img.at<section_t>(offset)) {}
   };

   template <Bits bits>
   class TextSection: public Section<bits> {
   public:
      using Content = std::list<SectionBlob<bits> *>;

      Content content;

      virtual ~TextSection();
      
      template <typename... Args>
      static TextSection<bits> *Parse(Args&&... args) { return new TextSection(args...); }
      
   private:
      TextSection(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };

   // TODO: Not yet complete
   template <Bits bits>
   class ZerofillSection: public Section<bits> {
   public:
      template <typename... Args>
      static ZerofillSection<bits> *Parse(Args&&... args) { return new ZerofillSection(args...); }
   private:
      ZerofillSection(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class SectionBlob {
   public:
      static SectionBlob<bits> *Parse(const Image& img, std::size_t offset, std::size_t vmaddr,
                                      ParseEnv<bits>& env);
   protected:
      SectionBlob(std::size_t vmaddr, ParseEnv<bits>& env);
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      std::vector<uint8_t> data;
      
      template <typename... Args>
      static DataBlob<bits> *Parse(Args&&... args) { return new DataBlob(args...); }
      
   private:
      // FIXME: this currently doesn't populate table with all possibilities
      DataBlob(const Image& img, std::size_t offset, std::size_t size, std::size_t vmaddr,
               ParseEnv<bits>& env):
         SectionBlob<bits>(vmaddr, env), data(&img.at<uint8_t>(offset),
                                              &img.at<uint8_t>(offset + size)) {}
   };
   
   template <Bits bits>
   class DataSection: public Section<bits> {
   public:
      std::vector<uint8_t> data;
      
      template <typename... Args>
      static DataSection<bits> *Parse(Args&&... args) { return new DataSection(args...); }
   private:
      DataSection(const Image& img, std::size_t offset, ParseEnv<bits>& env);
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
      SymbolPointerSection(const Image& img, std::size_t offset, ParseEnv<bits>& env);
   };   


   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t dstate;
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      const SectionBlob<bits> *memdisp; /*!< memory displacement pointee */
      const SectionBlob<bits> *brdisp;  /*!< branch displacement pointee */
      
      std::size_t size() const { return instbuf.size(); }

      template <typename... Args>
      static Instruction<bits> *Parse(Args&&... args) { return new Instruction(args...); }
      
   private:
      Instruction(const Image& img, std::size_t offset, std::size_t vmaddr, ParseEnv<bits>& env);
   };


};
