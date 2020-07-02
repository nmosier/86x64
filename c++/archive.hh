#pragma once

#include <vector>
#include <mach-o/loader.h>

#include "macho.hh"
#include "transform.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class Archive: public MachO {
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
      Segment<bits> *segment(std::size_t index) { return segments().at(index); }
      Segment<bits> *segment(const std::string& name);

      static Archive<bits> *Parse(const Image& img, std::size_t offset = 0) {
         return new Archive(img, offset);
      }
      virtual void Build() override;
      virtual void Emit(Image& img) const override;
      virtual ~Archive() override;

      template <typename... Args>
      void Insert(const SectionLocation<bits>& loc, Args&&... args) {
         loc.segment->Insert(loc, args...);
      }

      Archive<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Archive<opposite<bits>>(*this, env);
      }
      Archive<opposite<bits>> *Transform() const {
         TransformEnv<bits> env;
         return Transform(env);
      }

      void insert(SectionBlob<bits> *blob, const Location& loc, Relation rel);

   private:
      std::size_t total_size;

      Archive(const Image& img, std::size_t offset);
      Archive(const Archive<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

#if 0
      using SectionMap = std::map<std::size_t, Section<bits> *>;
      SectionMap section_vmaddr_map();
#endif
      
      template <Bits> friend class Archive;
   };   

}

