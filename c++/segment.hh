#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <optional>
#include <string>

extern "C" {
#include <xed-interface.h>
}

#include <mach-o/loader.h>

#include "image.hh"
#include "util.hh"
#include "lc.hh"
#include "parse.hh"
#include "section.hh"

namespace MachO {

   template <Bits bits>
   class Segment: public LoadCommand<bits> {
   public:
      using segment_command_t = select_type<bits, segment_command, segment_command_64>;
      using Sections = std::vector<Section<bits> *>;

      segment_command_t segment_command;
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

      static Segment<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      
      template <typename... Args>
      static Segment<bits> *Parse(Args&&... args) { return new Segment(args...); }

      virtual ~Segment() override;
      
   protected:
      Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      void Build_PAGEZERO(BuildEnv<bits>& env); /*!< for SEG_PAGEZERO segment */
   };

   template <Bits bits>
   class Segment_LINKEDIT: public Segment<bits> {
   public:
      template <typename... Args>
      static Segment_LINKEDIT<bits> *Parse(Args&&... args) { return new Segment_LINKEDIT(args...); }
      virtual void Build(BuildEnv<bits>& env) override;
      
   private:
      std::vector<LinkeditCommand<bits> *> linkedits;
      
      template <typename... Args>
      Segment_LINKEDIT(Args&&... args): Segment<bits>(args...) {}
   };

};
