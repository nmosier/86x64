#include <iostream>
#include <list>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include "macho.hh"
#include "util.hh"

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s path\n";

   if (argc != 2) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }

   MachO::Image img (argv[1]);
   
   MachO::MachO *macho = MachO::MachO::Parse(img);

   return 0;
}

namespace MachO {
   
   Image::Image(const char *path) {
      if ((fd = open(path, O_RDWR)) < 0) {
         throw cerror("open");
      }

      struct stat st;
      if (fstat(fd, &st) < 0) {
         close(fd);
         throw cerror("fstat");
      }
      filesize = st.st_size;
      
      if ((img = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL) {
         close(fd);
         throw cerror("mmap");
      }
   }
   
   Image::~Image() {
      munmap(img, filesize);
      close(fd);
   }

   void Image::resize(std::size_t newsize) {
      if (munmap(img, filesize) < 0) {
         img = NULL;
         throw cerror("munmap");
      }

      if (ftruncate(fd, newsize) < 0) {
         throw cerror("ftruncate");
      }

      filesize = newsize;

      if ((img = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL) {
         close(fd);
         throw cerror("mmap");
      }

   }


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
      archive->header = img.at<mach_header_t>(offset);

      offset += sizeof(archive->header);
      for (int i = 0; i < archive->header.ncmds; ++i) {
         LoadCommand<bits> *cmd = LoadCommand<bits>::Parse(img, offset);
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
   LoadCommand<bits> *LoadCommand<bits>::Parse(const Image& img, std::size_t offset) {
      load_command lc = img.at<load_command>(offset);

      switch (lc.cmd) {
      case LC_SEGMENT:
         if constexpr (bits == Bits::M32) {
               return Segment<Bits::M32>::Parse(img, offset);
            } else {
            throw("32-bit segment command in 64-bit binary");
         }
         
      case LC_SEGMENT_64:
         if constexpr (bits == Bits::M64) {
               return Segment<Bits::M64>::Parse(img, offset);
            } else {
            throw("64-bit segment command in 32-bit binary");
         }

      case LC_DYLD_INFO:
      case LC_DYLD_INFO_ONLY:
         return DyldInfo<bits>::Parse(img, offset);
         
      default:
         throw error("load command 0x%x not supported", lc.cmd);
      }
      
      // TODO
   }

   template <Bits bits>
   Segment<bits> *Segment<bits>::Parse(const Image& img, std::size_t offset) {
      Segment<bits> *segment = new Segment<bits>();
      segment->segment_command = img.at<segment_command_t>(offset);

      offset += sizeof(segment_command_t);
      for (int i = 0; i < segment->segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset);
         segment->sections.push_back(section);
         offset += section->size();
      }
      
      return segment;
   }

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset) {
      Section<bits> *sect = new Section<bits>();
      sect->sect = img.at<section_t>(offset);
      sect->data = std::vector<char>(&img.at<char>(sect->sect.offset),
                                     &img.at<char>(sect->sect.offset + sect->sect.size));
      return sect;
   }

   template <Bits bits>
   Segment<bits>::~Segment() {
      for (Section<bits> *ptr : sections) {
         delete ptr;
      }
   }

   template <Bits bits>
   DyldInfo<bits> *DyldInfo<bits>::Parse(const Image& img, std::size_t offset) {
      DyldInfo<bits> *dyld = new DyldInfo<bits>;
      dyld->dyld_info = img.at<dyld_info_command>(offset);
      dyld->rebase = std::vector<uint8_t>(img.at<uint8_t>(dyld->dyld_info.rebase_off),
                                          dyld->dyld_info.rebase_size);
      dyld->bind = std::vector<uint8_t>(img.at<uint8_t>(dyld->dyld_info.bind_off),
                                        dyld->dyld_info.bind_size);
      dyld->weak_bind = std::vector<uint8_t>(img.at<uint8_t>(dyld->dyld_info.weak_bind_off),
                                             dyld->dyld_info.weak_bind_size);
      dyld->lazy_bind = std::vector<uint8_t>(img.at<uint8_t>(dyld->dyld_info.lazy_bind_off),
                                             dyld->dyld_info.lazy_bind_size);
      dyld->export_info = std::vector<uint8_t>(img.at<uint8_t>(dyld->dyld_info.export_off),
                                               dyld->dyld_info.export_size);
      return dyld;
   }

   
}
