#include <iostream>
#include <list>
#include <vector>
#include <unordered_map>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mach-o/fat.h>
#include <mach-o/loader.h>

extern "C" {
#include <xed-interface.h>
}

#include "macho.hh"
#include "lc.hh"
#include "linkedit.hh"
#include "segment.hh"
#include "util.hh"
#include "build.hh"

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s path\n";

   if (argc < 2 || argc > 3) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }
   const char *out_path = argc == 2 ? "a.out" : argv[2];

   MachO::Image img(argv[1], O_RDONLY);
   MachO::Image out_img(out_path, O_RDWR | O_CREAT | O_TRUNC);

   xed_tables_init();
   
   MachO::MachO *macho = MachO::MachO::Parse(img);
   macho->Build();
   macho->Emit(out_img);
   (void) macho;

   return 0;
}

namespace MachO {

   MachO *MachO::Parse(const Image& img) {
      switch (img.at<uint32_t>(0)) {
      case MH_MAGIC:    return Archive<Bits::M32>::Parse(img);
      case MH_MAGIC_64: return Archive<Bits::M64>::Parse(img);
         
      case MH_CIGAM:
      case MH_CIGAM_64:
      case FAT_MAGIC:
      case FAT_CIGAM:
      case FAT_MAGIC_64:
      case FAT_CIGAM_64:
      default:
         throw error("opposite endianness unsupported");
      }

      return NULL;
   }

   template <Bits bits>
   Archive<bits> *Archive<bits>::Parse(const Image& img, std::size_t offset) {
      Archive<bits> *archive = new Archive<bits>();
      ParseEnv<bits> env(*archive);
      archive->header = img.at<mach_header_t>(offset);

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
      img.resize(total_size);

      /* emit header */
      img.at<mach_header_t>(0) = header;
      
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
