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
      virtual std::size_t Build(std::size_t offset) = 0;
      virtual void Build() override { Build(0); }
   };
   
   template <Bits bits>
   class Archive: public AbstractArchive {
   public:
      using LoadCommands = std::vector<LoadCommand<bits> *>;

      mach_header_t<bits> header;
      LoadCommands load_commands;

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }

      template <class Subclass, int64_t cmdval = -1>
      Subclass *subcommand() const {
         static_assert(std::is_base_of<LoadCommand<bits>, Subclass>());
         for (LoadCommand<bits> *lc : load_commands) {
            Subclass *subcommand = dynamic_cast<Subclass *>(lc);
            if (subcommand) { return subcommand; }
         }
         return nullptr;
      }
      
      template <class Subclass, int64_t cmdval = -1>
      std::vector<Subclass *> subcommands() const {
         static_assert(std::is_base_of<LoadCommand<bits>, Subclass>());
         std::vector<Subclass *> cmds;
         for (LoadCommand<bits> *lc : load_commands) {
            Subclass *cmd = dynamic_cast<Subclass *>(lc);
            if (cmd && (cmdval == -1 || cmdval == cmd->cmd())) {
               cmds.push_back(cmd);
            }
         }
         return cmds;
      }

      std::vector<Segment<bits> *> segments() { return subcommands<Segment<bits>>(); }
      std::vector<Segment<bits> *> segments() const { return subcommands<Segment<bits>>(); }
      Segment<bits> *segment(std::size_t index) { return segments().at(index); }
      Segment<bits> *segment(const std::string& name);

      static Archive<bits> *Parse(const Image& img, std::size_t offset = 0) {
         return new Archive(img, offset);
      }
      virtual std::size_t Build(std::size_t loc) override;
      virtual void Emit(Image& img) const override;
      virtual ~Archive() override;

      template <typename... Args>
      void Insert(const SectionLocation<bits>& loc, Args&&... args) {
         loc.segment->Insert(loc, args...);
      }

      std::size_t offset_to_vmaddr(std::size_t offset) const;

      Archive<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Archive<opposite<bits>>(*this, env);
      }
      Archive<opposite<bits>> *Transform() const {
         TransformEnv<bits> env;
         return Transform(env);
      }

      void insert(SectionBlob<bits> *blob, const Location& loc, Relation rel);

      template <template <Bits> class Blob>
      Blob<bits> *find_blob(std::size_t vmaddr) const {
         for (Segment<bits> *segment : segments()) {
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
      Archive(const Archive<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      
      template <Bits> friend class Archive;
   };   

}

