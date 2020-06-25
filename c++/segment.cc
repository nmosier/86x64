#include "segment.hh"
#include "util.hh"
#include "macho.hh"

namespace MachO {

   template <Bits bits>
   Segment<bits> *Segment<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      const char *segname = img.at<segment_command_t>(offset).segname;

      if (strcmp(segname, SEG_LINKEDIT) == 0) {
         return Segment_LINKEDIT<bits>::Parse(img, offset, env);
      } else {
         return new Segment(img, offset, env);
      }
   }

   
   template <Bits bits>
   Segment<bits>::Segment(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      segment_command = img.at<segment_command_t>(offset);

      env.current_segment = this;
      
      offset += sizeof(segment_command_t);
      for (int i = 0; i < segment_command.nsects; ++i) {
         Section<bits> *section = Section<bits>::Parse(img, offset, env);
         sections.push_back(section);
         offset += section->size();
      }
      
      env.current_segment = nullptr;
   }

   template <Bits bits>
   Segment<bits>::~Segment() {
      for (Section<bits> *ptr : sections) {
         delete ptr;
      }
   }   

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t sect = img.at<section_t>(offset);
      uint32_t flags = sect.flags;
      
      if ((flags & S_ATTR_PURE_INSTRUCTIONS)) {
         return TextSection<bits>::Parse(img, offset, env);
      } else if ((flags & S_ATTR_SOME_INSTRUCTIONS)) {
         throw error("segments with only 'some' instructions not supported");
      } else if (flags == S_LAZY_SYMBOL_POINTERS) {
         return LazySymbolPointerSection<bits>::Parse(img, offset, env);
      } else if (flags == S_NON_LAZY_SYMBOL_POINTERS) {
         return NonLazySymbolPointerSection<bits>::Parse(img, offset, env);
      } else if (flags == S_REGULAR || flags == S_CSTRING_LITERALS) {
         return DataSection<bits>::Parse(img, offset, env);
      } else if ((flags & S_ZEROFILL)) {
         return ZerofillSection<bits>::Parse(img, offset, env);
      }

      throw error("bad section flags (section %s)", sect.sectname);
   }

   template <Bits bits>
   const xed_state_t Instruction<bits>::dstate =
      {select_value(bits, XED_MACHINE_MODE_LEGACY_32, XED_MACHINE_MODE_LONG_64),
       select_value(bits, XED_ADDRESS_WIDTH_32b, XED_ADDRESS_WIDTH_32b)
      };

   template <Bits bits>
   Instruction<bits>::Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), memdisp(nullptr), brdisp(nullptr)
   {
      // xed_decoded_inst_t xedd;
      xed_error_enum_t err;
      
      xed_decoded_inst_zero_set_mode(&xedd, &dstate);
      xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);

      if ((err = xed_decode(&xedd, &img.at<uint8_t>(loc.offset), img.size() - loc.offset)) !=
          XED_ERROR_NONE) {
         throw error("%s: offset 0x%x: xed_decode: %s", __FUNCTION__, loc.offset,
                     xed_error_enum_t2str(err));
      }
      instbuf = std::vector<uint8_t>(&img.at<uint8_t>(loc.offset),
                                     &img.at<uint8_t>(loc.offset + xed_decoded_inst_get_length(&xedd)));
      const std::size_t refaddr = loc.vmaddr + xed_decoded_inst_get_length(&xedd);
      
      

      xed_operand_values_t *operands = xed_decoded_inst_operands(&xedd);
      unsigned int memops = xed_operand_values_number_of_memory_operands(operands);
      if (xed_operand_values_has_memory_displacement(operands)) {
         for (unsigned i = 0; i < memops; ++i) {
            xed_reg_enum_t basereg  = xed_decoded_inst_get_base_reg(operands,  i);
            xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(operands, i);
            if (basereg == select_value(bits, XED_REG_EIP, XED_REG_RIP) &&
                indexreg == XED_REG_INVALID)
               {
                  if (memdisp) {
                     fprintf(stderr,
                             "warning: %s: duplicate memdisp at offset 0x%jx, vmaddr 0x%jx\n",
                             __FUNCTION__, loc.offset, loc.vmaddr);
                     abort();
                  }

                  memidx = i;
                  
                  /* get memory displacement & reference address */
                  const ssize_t memdisp = xed_decoded_inst_get_memory_displacement(operands,
                                                                                        i);
                  const std::size_t targetaddr = refaddr + memdisp;

                  /* resolve pointer */
                  env.vmaddr_resolver.resolve(targetaddr, &this->memdisp);
               }
         }
      }

      /* Relative Branches */
      if (xed_operand_values_has_branch_displacement(operands)) {
         const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(operands);
         const size_t targetaddr = refaddr + brdisp;

         /* resolve */
         env.vmaddr_resolver.resolve(targetaddr, &this->brdisp);
      }
   }

   template <Bits bits>
   SectionBlob<bits> *TextSection<bits>::BlobParser(const Image& img, const Location& loc,
                                                    ParseEnv<bits>& env) {
      try {
         return Instruction<bits>::Parse(img, loc, env);
      } catch (...) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
   }

   template <Bits bits>
   std::size_t Segment<bits>::size() const {
      return sizeof(segment_command) + sections.size() * Section<bits>::size();
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const Location& loc, ParseEnv<bits>& env):
      segment(env.current_segment)
   {
      assert(segment);
      env.vmaddr_resolver.add(loc.vmaddr, this);
      env.offset_resolver.add(loc.offset, this);
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const Image& img, const Location& loc,
                                              ParseEnv<bits>& env):
      SymbolPointer<bits>(loc, env)
   {
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      std::size_t targetaddr = img.at<ptr_t>(loc.offset);
      env.vmaddr_resolver.resolve(targetaddr, (const SectionBlob<bits> **) &pointee);
   }

   template <Bits bits, class Elem>
   SectionT<bits, Elem>::~SectionT() {
      for (Elem *elem : content) {
         delete elem;
      }
   }

   template <Bits bits, class Elem>
   SectionT<bits, Elem>::SectionT(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                  Parser parser): Section<bits>(img, offset) {
      const std::size_t begin = this->sect.offset;
      const std::size_t end = begin + this->sect.size;
      std::size_t it = begin;
      std::size_t vmaddr = this->sect.addr;
      while (it != end) {
         Elem *elem = parser(img, Location(it, vmaddr), env);
         content.push_back(elem);
         it += elem->size();
         vmaddr += elem->size();
      }
   }

   template <Bits bits>
   std::string Section<bits>::name() const {
      return std::string(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
   }

   template <Bits bits>
   Section<bits> *Segment<bits>::section(const std::string& name) {
      for (Section<bits> *section : sections) {
         if (section->name() == name) {
            return section;
         }
      }
      return nullptr;
   }

   template <Bits bits>
   std::string Segment<bits>::name() const {
      return std::string(segment_command.segname, strnlen(segment_command.segname,
                                                          sizeof(segment_command.segname)));
   }

   template <Bits bits>
   std::size_t Segment<bits>::content_size() const {
      std::size_t total_size = 0;
      for (const Section<bits> *sect : sections) {
         total_size += sect->content_size();
      }
      return total_size;
   }

   template <Bits bits>
   void Segment<bits>::Build(BuildEnv<bits>& env) {
      assert(env.loc.vmaddr % PAGESIZE == 0);
      segment_command.nsects = sections.size();
      if (strcmp(segment_command.segname, SEG_PAGEZERO) == 0) {
         Build_PAGEZERO(env);
         return;
      }

      /* set segment start location */
      segment_command.fileoff = align_down(env.loc.offset, PAGESIZE);
      segment_command.vmaddr = env.loc.vmaddr;
      // segment_command.filesize = content_size() + env.loc.offset % PAGESIZE;
      // segment_command.vmsize = align_up<std::size_t>(segment_command.filesize, PAGESIZE);

      /* update env loc */
      env.loc.vmaddr += env.loc.offset % PAGESIZE;

      Build_content(env);

      
      /* post-conditions for vmaddr */
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);
      segment_command.filesize = env.loc.offset - segment_command.fileoff;
      segment_command.vmsize = env.loc.vmaddr - segment_command.vmaddr;
   }

   template <Bits bits>
   void Segment<bits>::Build_content(BuildEnv<bits>& env) {
      for (Section<bits> *sect : sections) {
         sect->Build(env);
      }
   }

   template <Bits bits>
   void Segment_LINKEDIT<bits>::Build_content(BuildEnv<bits>& env) {
      linkedits = env.archive.template subcommands<LinkeditCommand<bits>>();
      for (LinkeditCommand<bits> *linkedit : linkedits) {
         linkedit->Build_LINKEDIT(env);
      }
   }

   template <Bits bits>
   std::size_t Segment_LINKEDIT<bits>::content_size() const {
      std::size_t size = 0;
      for (LinkeditCommand<bits> *linkedit : linkedits) {
         size += linkedit->content_size();
      }
      return size;
   }
   
   template <Bits bits>
   void SectionBlob<bits>::Build(BuildEnv<bits>& env) {
      env.allocate(size(), loc);
   }

   template <Bits bits>
   void Section<bits>::Build(BuildEnv<bits>& env) {
      // env.align(sect.align);
      sect.size = content_size();
      loc(env.loc);
      Build_content(env);
      env.align(this->sect.align);
      fprintf(stderr, "[BUILD] segment %s, section %s @ offset 0x%zx, vmaddr 0x%zx\n", sect.segname,
              sect.sectname, loc().offset, loc().vmaddr);
   }

   template <Bits bits>
   void ZerofillSection<bits>::Build(BuildEnv<bits>& env) {
      // env.align(this->sect.align);
      this->sect.size = this->content_size();
      this->loc(Location(0, env.loc.vmaddr));
   }

   template <Bits bits, class Elem>
   void SectionT<bits, Elem>::Build_content(BuildEnv<bits>& env) {
      for (Elem *elem : content) {
         elem->Build(env);
      }
   }
   
   template <Bits bits, class Elem>
   std::size_t SectionT<bits, Elem>::content_size() const {
      std::size_t size = 0;
      for (const Elem *elem : content) {
         size += elem->size();
      }
      return align<bits>(size);
   }

   template <Bits bits>
   void Segment<bits>::Build_PAGEZERO(BuildEnv<bits>& env) {
      assert(env.loc.vmaddr == 0);
      segment_command.vmaddr = 0;
      segment_command.vmsize = vmaddr_start<bits>;
      segment_command.fileoff = 0;
      segment_command.filesize = 0;
      env.loc.vmaddr += vmaddr_start<bits>;
   }

   template <Bits bits>
   void Segment<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
#warning Segment::Build_LINKEDIT incomplete
   }

   template <Bits bits>
   void Segment<bits>::Build_default(BuildEnv<bits>& env) {
      /* set start offsets */
      segment_command.vmaddr = env.loc.vmaddr;
      segment_command.fileoff = align_down(env.loc.offset, PAGESIZE);

      /* adjust vmaddr */
      env.loc.vmaddr = env.loc.vmaddr + env.loc.offset - segment_command.fileoff;
      assert(env.loc.vmaddr % PAGESIZE == env.loc.offset % PAGESIZE);

      /* build sections */
      for (Section<bits> *sect : sections) {
         sect->Build(env);
      }

      /* compute and set sizes */
      segment_command.vmsize = align_up(env.loc.vmaddr, PAGESIZE) - segment_command.vmaddr;
      segment_command.filesize = segment_command.vmsize;

      /* realign vmaddr in env */
      env.loc.vmaddr = align_up(env.loc.vmaddr, PAGESIZE);

      fprintf(stderr, "[BUILD] segment %s @ offset 0x%zx, vmaddr 0x%zx\n", segment_command.segname,
              loc().offset, loc().vmaddr);
   }

   template <Bits bits>
   void Segment<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<segment_command_t>(offset) = segment_command;
      offset += sizeof(segment_command_t);
      
      for (const Section<bits> *sect : sections) {
         sect->Emit(img, offset);
         offset += sect->size();
      }
   }

   template <Bits bits>
   void Section<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<section_t>(offset) = sect;
      Emit_content(img, sect.offset);
   }
   
   template <Bits bits, class Elem>
   void SectionT<bits, Elem>::Emit_content(Image& img, std::size_t offset) const {
      for (const Elem *elem : content) {
         elem->Emit(img, offset);
         offset += elem->size();
      }
   }

   template <Bits bits>
   void DataBlob<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint8_t>(offset) = data;
   }

   template <Bits bits>
   void SymbolPointer<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<ptr_t>(offset) = raw_data();
   }

   template <Bits bits>
   void Instruction<bits>::Emit(Image& img, std::size_t offset) const {
      /* patch instruction */
      xed_decoded_inst_t xedd = this->xedd;
      std::vector<uint8_t> instbuf = this->instbuf;
      
      if (memdisp) {
         const unsigned width_bits =
            xed_decoded_inst_get_memory_displacement_width_bits(&xedd, memidx);
         const std::size_t disp = memdisp->loc.vmaddr -
            (ssize_t) (this->loc.vmaddr + size());
         xed_enc_displacement_t enc; // = {disp, width_bits};
         enc.displacement = disp;
         enc.displacement_bits = width_bits;
         if (!xed_patch_disp(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_disp: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx\n", __FUNCTION__, this->loc.offset, this->loc.vmaddr);
         }
      }
      
      if (brdisp) {
         const unsigned width_bits = xed_decoded_inst_get_branch_displacement_width_bits(&xedd);
         const std::size_t disp = brdisp->loc.vmaddr - (ssize_t) (this->loc.vmaddr + size());
         xed_encoder_operand_t enc;
         enc.u.brdisp = disp;
         enc.width_bits = width_bits;
         if (!xed_patch_relbr(&xedd, &*instbuf.begin(), enc)) {
            throw error("%s: xed_patch_relbr: failed to patch instruction at offset 0x%zx, " \
                        "vmaddr 0x%zx\n", __FUNCTION__, this->loc.offset, this->loc.vmaddr);
         }
      }
         
      /* emit instruction bytes */
      memcpy(&img.at<uint8_t>(offset), &*instbuf.begin(), instbuf.size());
   }

#if 0
   template <Bits bits>
   void Segment_LINKEDIT<bits>::Build(BuildEnv<bits>& env) {
      linkedits = env.archive.template subcommands<const LinkeditCommand<bits> *>();
   }

   template <Bits bits>
   void Segment_LINKEDIT<bits>::Emit(Image& img, std::size_t offset) const {
      /* find data offset of LC_SYMTAB, first linkedit entry */
   }
#endif
   
   template class Segment<Bits::M32>;
   template class Segment<Bits::M64>;

}
