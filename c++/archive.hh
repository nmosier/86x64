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
      // using mach_header_t = select_type<bits, mach_header, mach_header_64>;
      using LoadCommands = std::vector<LoadCommand<bits> *>;

      mach_header_t<bits> header;
      LoadCommands load_commands;

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }

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

      static Archive<bits> *Parse(const Image& img, std::size_t offset = 0);
      virtual void Build() override;
      virtual void Emit(Image& img) const override;
      virtual ~Archive() override;

      template <typename... Args>
      void Insert(const SectionLocation<bits>& loc, Args&&... args) {
         loc.segment->Insert(loc, args...);
      }

      template <Bits b1, Bits b2>
      Archive<b2> *Transform() const {
         TransformEnv<b1, b2> env;

         /* transform header */
         mach_header_t<b2> new_header;
         env(header, new_header);

         /* tranform load cmds */
         std::vector<LoadCommand<b2> *> new_lcs;
         for (const LoadCommand<b1> *lc : load_commands) {
            new_lcs.push_back(lc->Transform(env));
         }
         
         return new Archive<b2>(new_header, new_lcs);
      }

   private:
      std::size_t total_size;
      
      Archive() {}
      Archive(const mach_header_t<bits>& header, const LoadCommands& load_commands):
         header(header), load_commands(load_commands) {}
   };   

}
