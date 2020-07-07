#pragma once

#include <mach-o/loader.h>

#include "image.hh"
#include "util.hh"
#include "lc.hh"
#include "parse.hh"
#include "section.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using Sections = std::vector<Section<bits> *>;

      segment_command_t<bits> segment_command;
      Sections sections;
      unsigned id; /*!< assigned during build time */

      virtual uint32_t cmd() const override { return segment_command.cmd; }
      virtual std::size_t size() const override;
      std::string name() const;
      std::size_t vmaddr() const { return segment_command.vmaddr; }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void AssignID(BuildEnv<bits>& env) override {
         id = env.segment_counter();
      };
      virtual void Emit(Image& img, std::size_t offset) const override;
      Location loc() const { return Location(segment_command.fileoff, segment_command.vmaddr); }

      Section<bits> *section(const std::string& name);

      std::size_t offset_to_vmaddr(std::size_t offset) const;
      std::optional<std::size_t> try_offset_to_vmaddr(std::size_t offset) const;
      bool contains_offset(std::size_t offset) const;
      bool contains_vmaddr(std::size_t vmaddr) const;

      template <template <Bits> class Blob>
      Blob<bits> *find_blob(std::size_t vmaddr) const {
         for (Section<bits> *section : sections) {
            if (section->contains_vmaddr(vmaddr)) {
               return section->template find_blob<Blob>(vmaddr);
            }
         }
         std::stringstream ss;
         ss << "no section blob at " << std::hex << vmaddr;
         throw std::invalid_argument(ss.str());
      }
      
      static Segment<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) override {
         env.current_segment = this;
         for (auto section : sections) {
            section->Parse1(img, env);
         }
         env.current_segment = nullptr;
      }
      virtual void Parse2(ParseEnv<bits>& env) override {
         env.current_segment = this;
         for (auto section : sections) {
            section->Parse2(env);
         }
         env.current_segment = nullptr;
      }


      
      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }
      virtual ~Segment() override;

      template <typename... Args>
      void Insert(const SectionLocation<bits>& loc, Args&&... args) {
         loc.section->Insert(loc, args...);
      }

      void insert(SectionBlob<bits> *blob, const Location& loc, Relation rel);

   protected:
      Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      Segment(const Segment<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      virtual Segment<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Segment<opposite<bits>>(*this, env);
      }

      void Build_PAGEZERO(BuildEnv<bits>& env); /*!< for SEG_PAGEZERO segment */

      template <Bits> friend class Segment;
   };

   template <Bits bits>
   class Segment_LINKEDIT: public Segment<bits> {
   public:
      template <typename... Args>
      static Segment_LINKEDIT<bits> *Parse(Args&&... args) { return new Segment_LINKEDIT(args...); }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual Segment<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Segment_LINKEDIT<opposite<bits>>(*this, env);
      }
     
   private:
      std::vector<LinkeditCommand<bits> *> linkedits;
      
      template <typename... Args>
      Segment_LINKEDIT(Args&&... args): Segment<bits>(args...) {}

      template <Bits> friend class Segment_LINKEDIT;
   };

};
