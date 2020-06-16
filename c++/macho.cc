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
#include "util.hh"

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s path\n";

   if (argc != 2) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }

   MachO::Image img (argv[1]);
   
   MachO::MachO *macho = MachO::MachO::Parse(img);
   (void) macho;

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

      case LC_SYMTAB:
         return Symtab<bits>::Parse(img, offset);

      case LC_DYSYMTAB:
         return Dysymtab<bits>::Parse(img, offset);

      case LC_LOAD_DYLINKER:
         return DylinkerCommand<bits>::Parse(img, offset);

      case LC_UUID:
         return UUID<bits>::Parse(img, offset);

      case LC_BUILD_VERSION:
         return BuildVersion<bits>::Parse(img, offset);

      case LC_SOURCE_VERSION:
         return SourceVersion<bits>::Parse(img, offset);

      case LC_MAIN:
         return EntryPoint<bits>::Parse(img, offset);

      case LC_LOAD_DYLIB:
         return DylibCommand<bits>::Parse(img, offset);

      case LC_FUNCTION_STARTS:
      case LC_DATA_IN_CODE:
      case LC_CODE_SIGNATURE:
         return LinkeditData<bits>::Parse(img, offset);
         
      default:
         throw error("load command 0x%x not supported", lc.cmd);
      }
      
      // TODO
   }

   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset) {
      segment_command = img.at<segment_command_t>(offset);

      offset += sizeof(segment_command_t);
      for (int i = 0; i < segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset);
         sections.push_back(section);
         offset += section->size();
      }
   }

#if 0
   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset) {
      Section<bits> *sect = new Section<bits>();
      sect->sect = img.at<section_t>(offset);
      sect->data = std::vector<char>(&img.at<char>(sect->sect.offset),
                                     &img.at<char>(sect->sect.offset + sect->sect.size));
      return sect;
   }
#endif

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
      dyld->rebase = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.rebase_off),
                                          &img.at<uint8_t>(dyld->dyld_info.rebase_off +
                                                           dyld->dyld_info.rebase_size));
      dyld->bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.bind_off),
                                        &img.at<uint8_t>(dyld->dyld_info.bind_off +
                                                         dyld->dyld_info.bind_size));
      dyld->weak_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.weak_bind_off),
                                             &img.at<uint8_t>(dyld->dyld_info.weak_bind_off +
                                                              dyld->dyld_info.weak_bind_size));
      dyld->lazy_bind = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.lazy_bind_off),
                                             &img.at<uint8_t>(dyld->dyld_info.lazy_bind_off +
                                                              dyld->dyld_info.lazy_bind_size));
      dyld->export_info = std::vector<uint8_t>(&img.at<uint8_t>(dyld->dyld_info.export_off),
                                               &img.at<uint8_t>(dyld->dyld_info.export_off +
                                                                dyld->dyld_info.export_size));
      return dyld;
   }

   template <Bits bits>
   Symtab<bits>::Symtab(const Image& img, std::size_t offset):
      symtab(img.at<symtab_command>(offset)) {
      /* construct strings */
      const std::size_t strbegin = symtab.stroff;
      const std::size_t strend = strbegin + symtab.strsize;
      std::unordered_map<std::size_t, std::string *> off2str = {{0, nullptr}};
      for (std::size_t strit = strbegin + strnlen(&img.at<char>(strbegin), strend - strbegin) + 1;
           strit < strend; ) {
         const std::size_t strrem = strend - strit;
         std::size_t len = strnlen(&img.at<char>(strit), strrem);
         if (len == strrem) {
            throw error("string '%*s...' runs past end of string table",
                           strrem, &img.at<char>(strit));
         } else if (len > 0) {
            std::string *s = new std::string(&img.at<char>(strit));
            strs.push_back(s);
            off2str[strit - strbegin] = s;
            strit += len;
         }
         ++strit; /* skip null byte */
      }

      /* construct symbols */
      for (uint32_t i = 0; i < symtab.nsyms; ++i) {
         syms.push_back(Nlist<bits>::Parse(img, symtab.symoff + i * Nlist<bits>::size(), off2str));
      }

   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Image& img, std::size_t offset,
                      const std::unordered_map<std::size_t, std::string *>& off2str) {
      nlist = img.at<nlist_t>(offset);
      if (off2str.find(nlist.n_un.n_strx) == off2str.end()) {
         throw error("nlist offset 0x%x does not point to beginning of string", nlist.n_un.n_strx);
      }
      string = off2str.at(nlist.n_un.n_strx);
   }

   template <Bits bits>
   Dysymtab<bits>::Dysymtab(const Image& img, std::size_t offset):
      dysymtab(img.at<dysymtab_command>(offset)),
      indirectsyms(std::vector<uint32_t>(&img.at<uint32_t>(dysymtab.indirectsymoff),
                                         &img.at<uint32_t>(dysymtab.indirectsymoff) +
                                         dysymtab.nindirectsyms)) {}

   template <Bits bits>
   DylinkerCommand<bits>::DylinkerCommand(const Image& img, std::size_t offset):
      dylinker(img.at<dylinker_command>(offset)) {
      if (dylinker.name.offset > dylinker.cmdsize) {
         throw error("dylinker name starts past end of command");
      }
      const std::size_t maxstrlen = dylinker.cmdsize - dylinker.name.offset;
      const std::size_t slen = strnlen(&img.at<char>(offset + dylinker.name.offset), maxstrlen);
      const char *strbegin = &img.at<char>(offset + dylinker.name.offset);
      name = std::string(strbegin, strbegin + slen);
   }

   template <Bits bits>
   std::size_t DylinkerCommand<bits>::size() const {
      return align_up(sizeof(dylinker_command) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   BuildVersion<bits>::BuildVersion(const Image& img, std::size_t offset):
      build_version(img.at<build_version_command>(offset)) {
      for (int i = 0; i < build_version.ntools; ++i) {
         tools.push_back(BuildToolVersion<bits>::Parse(img, offset +
                                                       sizeof(build_version) +
                                                       i * BuildToolVersion<bits>::size()));
      }
   }

   template <Bits bits>
   std::size_t BuildVersion<bits>::size() const {
      return sizeof(build_version) + BuildToolVersion<bits>::size() * tools.size();
   }

   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const Image& img, std::size_t offset):
      entry_point(img.at<entry_point_command>(offset)) {}

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(const Image& img, std::size_t offset):
      dylib_cmd(img.at<dylib_command>(offset)) {
      const std::size_t stroff = dylib_cmd.dylib.name.offset;
      if (stroff > dylib_cmd.cmdsize) {
         throw error("dynamic library name starts past end of dylib command");
      }
      const std::size_t strrem = dylib_cmd.cmdsize - stroff;
      const std::size_t len = strnlen(&img.at<char>(offset + stroff), strrem);
      name = std::string(&img.at<char>(offset + stroff), &img.at<char>(offset + stroff + len));
   }

   template <Bits bits>
   std::size_t DylibCommand<bits>::size() const {
      return align_up(sizeof(dylib_cmd) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   LinkeditData<bits>::LinkeditData(const Image& img, std::size_t offset):
      linkedit(img.at<linkedit_data_command>(offset)) {
      function_starts = std::vector<uint8_t>(&img.at<uint8_t>(linkedit.dataoff),
                                             &img.at<uint8_t>(linkedit.dataoff +
                                                              linkedit.datasize));
   }

   template <Bits bits>
   std::size_t Segment<bits>::size() const {
      return sizeof(segment_command) + sections.size() * Section<bits>::size();
   }

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset) {
      section_t sect = img.at<section_t>(offset);
      uint32_t flags = sect.flags;
      
      if ((flags & S_ATTR_PURE_INSTRUCTIONS)) {
         return TextSection<bits>::Parse(img, offset);
      } else if ((flags & S_ATTR_SOME_INSTRUCTIONS)) {
         throw error("segments with only 'some' instructions not supported");
      } else if ((flags & (S_NON_LAZY_SYMBOL_POINTERS | S_LAZY_SYMBOL_POINTERS))) {
         return SymbolPointerSection<bits>::Parse(img, offset);
      } else if ((flags & (S_REGULAR | S_CSTRING_LITERALS))) {
         return DataSection<bits>::Parse(img, offset);
      } else if ((flags & S_ZEROFILL)) {
         throw error("zerofill segments not supported yet");
      }

      throw error("bad section flags (offset 0x%zx)", offset);
   }

   template <Bits bits>
   SymbolPointerSection<bits>::SymbolPointerSection(const Image& img, std::size_t offset):
      Section<bits>(img, offset) {
      pointers = std::vector<pointer_t>(&img.at<pointer_t>(this->sect.offset),
                                        &img.at<pointer_t>(this->sect.offset + this->sect.size));
   }

   template <Bits bits>
   const xed_state_t Instruction<bits>::dstate =
      {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
       select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_64b)
      };

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, std::size_t offset) {
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      xed_error_enum_t err;
      if ((err = xed_decode(&xedd, &img.at<uint8_t>(offset),
                            img.size() - offset)) != XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, offset,
                     xed_error_enum_t2str(err));
      }

      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(offset),
                                     &img.at<uint8_t>(offset + xed_decoded_inst_get_length(&xedd)));
   }

   template <Bits bits>
   TextSection<bits>::TextSection(const Image& img, std::size_t offset): Section<bits>(img, offset)
   {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      for (std::size_t it = begin; it < end; ) {
         Instruction<bits> *ptr = Instruction<bits>::Parse(img, it);
         instructions.push_back(ptr);
         it += ptr->size();
      }
   }

         
   
}
