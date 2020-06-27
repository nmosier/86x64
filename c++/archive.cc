#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"

namespace MachO {

   template <Bits bits>
   Archive<bits> *Archive<bits>::Parse(const Image& img, std::size_t offset) {
      Archive<bits> *archive = new Archive<bits>();
      ParseEnv<bits> env(*archive);
      archive->header = img.at<mach_header_t<bits>>(offset);

      offset += sizeof(archive->header);
      for (int i = 0; i < archive->header.ncmds; ++i) {
         LoadCommand<bits> *cmd = LoadCommand<bits>::Parse(img, offset, env);
         archive->load_commands.push_back(cmd);
         offset += cmd->size();
      }

      return archive;
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

   template class Archive<Bits::M32>;
   template class Archive<Bits::M64>;

}
