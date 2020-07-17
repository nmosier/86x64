#pragma once

#include <vector>
#include <sstream>
#include <mach-o/loader.h>

#include "macho.hh"
#include "transform.hh"
#include "types.hh"
#include "segment.hh"

namespace MachO {

   class AbstractArchive: public MachO {
   public:
      
      
      static AbstractArchive *Parse(const Image& img, std::size_t offset);
      virtual void Build() override { Build(0); }
      virtual std::size_t Build(std::size_t offset) = 0;
      // virtual std::size_t Build(std::size_t offset, std::size_t vmaddr) = 0;
   };
   
   template <Bits b>
   class Archive: public AbstractArchive {
   public:
      using LoadCommands = std::vector<LoadCommand<b> *>;

      mach_header_t<b> header;
      LoadCommands load_commands;
      std::size_t vmaddr = vmaddr_start<b>; /*!< start vmaddr */

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }
      virtual Bits bits() const override { return b; }

      template <template <Bits> class Subclass, int64_t cmdval = -1>
      Subclass<b> *subcommand() const {
         static_assert(std::is_base_of<LoadCommand<b>, Subclass<b>>());
         for (LoadCommand<b> *lc : load_commands) {
            Subclass<b> *subcommand = dynamic_cast<Subclass<b> *>(lc);
            if (subcommand) { return subcommand; }
         }
         return nullptr;
      }
      
      template <template <Bits> class Subclass, int64_t cmdval = -1>
      std::vector<Subclass<b> *> subcommands() const {
         static_assert(std::is_base_of<LoadCommand<b>, Subclass<b>>());
         std::vector<Subclass<b> *> cmds;
         for (LoadCommand<b> *lc : load_commands) {
            Subclass<b> *cmd = dynamic_cast<Subclass<b> *>(lc);
            if (cmd && (cmdval == -1 || cmdval == cmd->cmd())) {
               cmds.push_back(cmd);
            }
         }
         return cmds;
      }

      std::vector<Segment<b> *> segments() { return subcommands<Segment>(); }
      std::vector<Segment<b> *> segments() const { return subcommands<Segment>(); }
      Segment<b> *segment(std::size_t index) { return segments().at(index); }
      Segment<b> *segment(const std::string& name);

      std::vector<Section<b> *> sections() const;
      Section<b> *section(uint8_t index) const;
      Section<b> *section(const std::string& name) const;

      static Archive<b> *Parse(const Image& img, std::size_t offset = 0) {
         return new Archive(img, offset);
      }

      virtual std::size_t Build(std::size_t offset) override;

      virtual void Emit(Image& img) const override;
      virtual ~Archive() override;

      template <typename... Args>
      void Insert(const SectionLocation<b>& loc, Args&&... args) {
         loc.segment->Insert(loc, args...);
      }

      std::size_t offset_to_vmaddr(std::size_t offset) const;
      std::optional<size_t> try_offset_to_vmaddr(std::size_t offset) const;

      Archive<opposite<b>> *Transform(TransformEnv<b>& env) const {
         return new Archive<opposite<b>>(*this, env);
      }
      Archive<opposite<b>> *Transform() const {
         TransformEnv<b> env;
         return Transform(env);
      }

      void insert(SectionBlob<b> *blob, const Location& loc, Relation rel);
      void remove_commands(uint32_t cmd);
      
      template <template <Bits> class Blob>
      Blob<b> *find_blob(std::size_t vmaddr) const {
         for (Segment<b> *segment : segments()) {
            if (segment->contains_vmaddr(vmaddr)) {
               return segment->template find_blob<Blob>(vmaddr);
            }
         }
         std::stringstream ss;
         ss << "vmaddr " << std::hex << vmaddr << " not in any segment";
         throw std::invalid_argument(ss.str());
      }

   private:
      std::size_t total_size;

      Archive(const Image& img, std::size_t offset);
      Archive(const Archive<opposite<b>>& other, TransformEnv<opposite<b>>& env);
      
      template <Bits> friend class Archive;
   };   

}

