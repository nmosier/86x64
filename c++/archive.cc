#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"

namespace MachO {

   template <Bits bits>
   Archive<bits>::Archive(const Image& img, std::size_t offset):
      header(img.at<mach_header_t<bits>>(offset))
   {
      ParseEnv<bits> env(*this);
      offset += sizeof(header);
      for (int i = 0; i < header.ncmds; ++i) {
         LoadCommand<bits> *cmd = LoadCommand<bits>::Parse(img, offset, env);
         load_commands.push_back(cmd);
         offset += cmd->size();
      }

      for (LoadCommand<bits> *cmd : load_commands) {
         cmd->Parse1(img, env);
      }
   }

   template <Bits bits>
   Archive<bits>::~Archive() {
      for (LoadCommand<bits> *lc : load_commands) {
         delete lc;
      }
   }

   template <Bits bits>
   Segment<bits> *Archive<bits>::segment(const std::string& name) {
      for (Segment<bits> *seg : segments()) {
         if (name == seg->name()) {
            return seg;
         }
      }
      return nullptr;
   }

   template <Bits bits>
   void Archive<bits>::Build() {
      BuildEnv<bits> env(*this, Location(0, 0));

      env.allocate(sizeof(header));
      
      /* count number of load commands */
      header.ncmds = load_commands.size();

      /* compute size of commands */
      header.sizeofcmds = 0;
      for (LoadCommand<bits> *lc : load_commands) {
         header.sizeofcmds += lc->size();
      }

      env.allocate(header.sizeofcmds);

      /* assign IDs */
      for (LoadCommand<bits> *lc : load_commands) {
         lc->AssignID(env);
      }
      
      /* build each command */
      for (LoadCommand<bits> *lc : load_commands) {
         lc->Build(env);
      }

      total_size = env.loc.offset;
   }

   template <Bits bits>
   void Archive<bits>::Emit(Image& img) const {
      /* emit header */
      img.at<mach_header_t<bits>>(0) = header;
      
      /* emit load commands */
      std::size_t offset = sizeof(header);
      for (LoadCommand<bits> *lc : load_commands) {
         lc->Emit(img, offset);
         offset += lc->size();
      }
   }

   template <Bits bits>
   Archive<bits>::Archive(const Archive<opposite<bits>>& other, TransformEnv<opposite<bits>>& env)
   {
      env(other.header, header);
      for (const auto lc : other.load_commands) {
         load_commands.push_back(lc->Transform(env));
      }
   }

   template class Archive<Bits::M32>;
   template class Archive<Bits::M64>;

}
