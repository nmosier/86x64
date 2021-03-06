#include <clang-c/Index.h>
#include <sstream>

#include "emit.hh"
#include "util.hh"
#include "typeinfo.hh"
#include "typeconv.hh"
#include "abigen.hh"

static size_t alignof_record(CXType type, arch a);

namespace {

   namespace {

      template <arch a, typename T_i386, typename T_x86_64>
      auto select_type_func() {
         if constexpr (a == arch::i386) {
               return T_i386();
            }
         if constexpr (a == arch::x86_64) {
               return T_x86_64();
            }
      }


   }

   template <arch a, typename T_i386, typename T_x86_64>
   using select_type = decltype(select_type_func<a, T_i386, T_x86_64>());
   
   template <arch a>
   using ul_t = select_type<a, uint32_t, uint64_t>;

   template <arch a>
   using l_t = select_type<a, int32_t, int64_t>;


}

record_decl::record_decl(CXCursor cursor): cursor(cursor) {
   populate_fields();
}

record_decl::record_decl(CXType type): cursor(clang_getTypeDeclaration(type)) {
   populate_fields();
}

void record_decl::populate_fields() {
   for_each(cursor,
            [&] (CXCursor c, CXCursor p) {
               switch (clang_getCursorKind(c)) {
               case CXCursor_FieldDecl:
                  {
                     CXType type = clang_getCanonicalType(clang_getCursorType(c));
                     field_types.push_back(type);
                  }
                  break;

               case CXCursor_PackedAttr:
                  packed = true;
                  break;

               case CXCursor_StructDecl:
                  /* ignore */
                  break;

               default: abort();
               }
               return CXChildVisit_Continue;
            });
}

void conversion::push(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst) {
   src.push(); dst.push(); data.push();
   emit_inst(os, "push", loc.reg.reg_q);
}

void conversion::pop(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst) {
   src.pop();  dst.pop();  data.pop();
   emit_inst(os, "pop", loc.reg.reg_q);
}

void conversion::push(std::ostream& os, const SSELocation& loc, Location& src, Location& dst) {
   src.push(); dst.push(); data.push();
   emit_inst(os, "sub", "rsp", 8);
   emit_inst(os, "movsd", "[rsp]", loc.op(reg_width::Q));
}

void conversion::pop(std::ostream& os, const SSELocation& loc, Location& src, Location& dst) {
   src.pop(); dst.pop(); data.pop();
   emit_inst(os, "movsd", loc.op(reg_width::Q), "[rsp]");
   emit_inst(os, "add", "rsp", 8);
}

void conversion::convert_int(std::ostream& os, CXTypeKind type_kind, const Location& src,
                             const Location& dst) {
   reg_width from_width = get_type_width(type_kind, from_arch);
   reg_width to_width = get_type_width(type_kind, to_arch);
   
   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      const RegisterLocation tmp_reg(rax);
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_reg, tmp_src, tmp_dst);
      convert_int(os, type_kind, tmp_src, tmp_reg);
      convert_int(os, type_kind, tmp_reg, tmp_dst);
      pop(os, tmp_reg, tmp_src, tmp_dst);
   } else if (dst.kind() == Location::Kind::MEM && get_type_signed(type_kind) &&
              from_width < to_width) {
      /* movsx qword <src>, dword <src>
       * mov <dst>, <src>
       */
      emit_inst(os, "movsx", src.op(to_width), src.op(from_width));
      emit_inst(os, "mov", dst.op(to_width), src.op(to_width));
   } else {
      const char *opcode;

      if (from_width < to_width) {
         if (get_type_signed(type_kind)) {
            opcode = "movsx";
         } else {
            to_width = from_width;
            opcode = "mov";
         }
      } else {
         opcode = "mov";
         from_width = to_width;
      }

      emit_inst(os, opcode, dst.op(to_width), src.op(from_width));
   }
}

void conversion::convert_real(std::ostream& os, CXTypeKind type_kind, const Location& src,
                              const Location& dst) {
   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      SSELocation tmp_loc = {0};
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_loc, tmp_src, tmp_dst);
      convert_real(os, type_kind, src, tmp_loc);
      convert_real(os, type_kind, tmp_loc, dst);
      pop(os, tmp_loc, tmp_src, tmp_dst);
   } else { 
      const reg_width to_width = get_type_width(type_kind, to_arch);
      const reg_width from_width = get_type_width(type_kind, from_arch);
      std::stringstream opcode;
      if (to_width == from_width) {
         opcode << "mov" << reg_width_to_sse(to_width);
      } else {
         opcode << "cvt" << reg_width_to_sse(from_width) << "2" << reg_width_to_sse(to_width);
      }
      emit_inst(os, opcode.str(), dst.op(to_width), src.op(from_width));
   }
}

void conversion::convert_void_pointer(std::ostream& os, const Location& src,
                                      const Location& dst) {
   convert_int(os, CXType_Pointer, src, dst);
}

void conversion::convert(std::ostream& os, CXType type, const Location& src, const Location& dst) {
   os << "\t; convert '" << to_string(type) << "'" << std::endl;
   switch (type.kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
      throw std::invalid_argument("invalid type");
      
   case CXType_Void:
      return;
      
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_UShort:
   case CXType_UInt:
   case CXType_ULong:
   case CXType_ULongLong:
   case CXType_SChar:
   case CXType_Char_S:
   case CXType_Short:
   case CXType_Int:
   case CXType_Long:
   case CXType_LongLong:
   case CXType_Enum:
      convert_int(os, type.kind, src, dst);
      break;
      
   case CXType_Float:
   case CXType_Double:
   case CXType_LongDouble:
      convert_real(os, type.kind, src, dst);
      return;
      
   case CXType_Pointer:
      convert_pointer(os, clang_getPointeeType(type), src, dst);
      break;
      
   case CXType_BlockPointer:
      // TODO
      convert_int(os, CXType_Pointer, src, dst);
      break;

   case CXType_ConstantArray:
      convert_constant_array(os, type,
                             dynamic_cast<const MemoryLocation&>(src),
                             dynamic_cast<const MemoryLocation&>(dst));
      break;
      
   case CXType_Record:
      convert_record(os, type,
                     dynamic_cast<const MemoryLocation&>(src),
                     dynamic_cast<const MemoryLocation&>(dst));
      break;

   case CXType_FunctionProto:
      break;
      
   default: abort();
   }
}

void conversion::convert_constant_array(std::ostream& os, CXType array, MemoryLocation src,
                                        MemoryLocation dst) {
   /* convert data */
   const long long arrlen = clang_getArraySize(array);
   assert(arrlen >= 1);
   const CXType elem = clang_getArrayElementType(array);
   const std::string loop = label();
   
   /*   push rcx
    *   push rdi
    *   push rsi
    *   mov ecx, <arrlen>
    *   lea rdi, [<dst>]
    *   lea rsi, [<src>]
    * loop:
    *   <move>
    *   add rdi, <size64>
    *   add rsi, <size32>
    * entry:
    *   dec ecx
    *   jnz loop
    *   pop rsi
    *   pop rdi
    *   pop rcx
    */

   push(os, rcx, src, dst);
   push(os, rdi, src, dst);
   push(os, rsi, src, dst);
   
   emit_inst(os, "mov", "ecx", arrlen);
   emit_inst(os, "lea", "rdi", dst.op());
   emit_inst(os, "lea", "rsi", src.op());

   os << loop << ":";
   {
      MemoryLocation src(rsi, 0);
      MemoryLocation dst(rdi, 0);
      
      convert(os, elem, src, dst);
      emit_inst(os, "add", "rdi", sizeof_type(elem, arch::x86_64));
      emit_inst(os, "add", "rsi", sizeof_type(elem, arch::i386));
      emit_inst(os, "dec", "ecx");
      emit_inst(os, "jnz", loop);
   }

   pop(os, rsi, src, dst);
   pop(os, rdi, src, dst);
   pop(os, rcx, src, dst);
}

void conversion::convert_record(std::ostream& os, CXType record, MemoryLocation src,
                                MemoryLocation dst) {
   record_decl decl(record);
   assert(decl.cursor.kind == CXCursor_StructDecl);
   for (CXType field_type : decl.field_types) {
      src.align_field(field_type, from_arch);
      dst.align_field(field_type, to_arch);
      
      convert(os, field_type, src, dst);

#if 0
      std::cerr << to_string(record) << "," << to_string(field_type) << "," << src.index
                << "," << dst.index << std::endl;
#endif

      /* update src, dst */
      src += sizeof_type(field_type, from_arch);
      dst += sizeof_type(field_type, to_arch);
   }
}

void conversion::convert_pointer(std::ostream& os, CXType pointee, const Location& src_,
                                 const Location& dst_) {
   if (ignore_structs.find(to_string(pointee)) != ignore_structs.end()) {
      convert_int(os, CXType_Pointer, src_, dst_);
      return;
   }

   std::unique_ptr<Location> srcp(src_.copy());
   std::unique_ptr<Location> dstp(dst_.copy());
   Location& src = *srcp;
   Location& dst = *dstp;

   if (allocate) {
      /* allocate new pointer to stack data into reg_dst and mem_dst */
      RegisterLocation reg_dst(r11);
      MemoryLocation mem_dst(r11, 0);
      push(os, reg_dst, src, dst);
      {
         data.align(pointee, to_arch);
         emit_inst(os, "lea", "r11", data.op());
         data += sizeof_type(pointee, to_arch);
      
         /* load reg_dst to dst */
         convert_int(os, CXType_Pointer, reg_dst, dst);
         
         /* load src pointer into reg_src */
         RegisterLocation reg_src(r12);
         MemoryLocation mem_src(r12, 0);
         push(os, reg_src, src, dst);
         {
            convert_int(os, CXType_Pointer, src, reg_src);

            /* convert */
            convert(os, pointee, mem_src, mem_dst);
         }
         pop(os, reg_src, src, dst);
      }
      pop(os, reg_dst, src, dst);
      
   } else {
      /* let dst pointer be */
      RegisterLocation reg_src(r12);
      MemoryLocation mem_src(r12, 0);
      push(os, reg_src, src, dst);
      {
         RegisterLocation reg_dst(r11);
         MemoryLocation mem_dst(r11, 0);
         push(os, reg_dst, src, dst);
         {
            data.align(pointee, from_arch);
            emit_inst(os, "lea", "r12", data.op());
            data += sizeof_type(pointee, from_arch);
            
            convert_int(os, CXType_Pointer, dst, reg_dst);
            
            /* convert underlying data */
            convert(os, pointee, mem_src, mem_dst);
         }
         pop(os, reg_dst, src, dst);
      }
      pop(os, reg_src, src, dst);
   }
}

/* SIZEOF */


static size_t sizeof_record(CXType type, arch a);

size_t sizeof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Bool:
   case CXType_Char_U:
   case CXType_UChar:
   case CXType_Char_S:
   case CXType_SChar:
      return 1;
   case CXType_UShort:
   case CXType_Short:
      return 2;
   case CXType_UInt:
   case CXType_Int:
   case CXType_Enum:
      return 4;
   case CXType_ULong:
   case CXType_Long:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ULongLong:
   case CXType_LongLong:
      return 8;
   case CXType_Float:
      return sizeof(float);
   case CXType_Double:
      return sizeof(double);
   case CXType_LongDouble:
      return sizeof(long double);
   case CXType_Pointer:
   case CXType_BlockPointer:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ConstantArray:
      return sizeof_type(clang_getArrayElementType(type), a) * clang_getArraySize(type);
   case CXType_Record:
      return sizeof_record(type, a);

   case CXType_FunctionProto:
      // TODO: May need to address this case in the future.
      return sizeof_type(CXType_Pointer, a);

   case CXType_Void:
      return 0;

   case CXType_IncompleteArray:
      abort();
      
   default:
      abort();
   }
}

size_t sizeof_type(CXTypeKind type_kind, arch a) {
   switch (type_kind) {
   case CXType_Bool:
   case CXType_Char_U:
   case CXType_UChar:
   case CXType_Char_S:
   case CXType_SChar:
      return 1;
   case CXType_UShort:
   case CXType_Short:
      return 2;
   case CXType_UInt:
   case CXType_Int:
   case CXType_Enum:
      return 4;
   case CXType_ULong:
   case CXType_Long:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   case CXType_ULongLong:
   case CXType_LongLong:
      return 8;
   case CXType_Float:
      return sizeof(float);
   case CXType_Double:
      return sizeof(double);
   case CXType_LongDouble:
      return sizeof(long double);
   case CXType_Pointer:
   case CXType_BlockPointer:
      switch (a) {
      case arch::i386: return 4;
      case arch::x86_64: return 8;
      default: abort();
      }
   default:
      abort();
   }
}

static size_t sizeof_union(CXType type, arch a);
static size_t sizeof_struct(CXType type, arch a);
static size_t sizeof_record(CXType type, arch a) {
   switch (clang_getCursorKind(clang_getTypeDeclaration(type))) {
   case CXCursor_UnionDecl:
      return sizeof_union(type, a);
   case CXCursor_StructDecl:
      return sizeof_struct(type, a);
   default: abort();
   }
}

static size_t sizeof_union(CXType type, arch a) {
   record_decl decl(type);
   assert(!decl.packed);
   
   size_t size = 0;

   for (CXType field_type : decl.field_types) {
      const size_t field_size = sizeof_type(field_type, a);
      size = std::max(size, field_size);
   }

   const size_t align = alignof_record(type, a);
   size = align_up(size, align);

   return size;
}

static size_t sizeof_struct(CXType type, arch a) {
   record_decl decl(type);
   size_t size = 0;

   for (CXType field_type : decl.field_types) {
      const size_t field_size = sizeof_type(field_type, a);
      if (!decl.packed) {
         const size_t field_align = alignof_type(field_type, a);
         size = align_up(size, field_align) + field_size;
      }
   }

   if (!decl.packed) {
      const size_t align = alignof_type(type, a);
      size = align_up(size, align);
   }
   
   return size;
}

int sizeof_type_archcmp(CXType type) {
   return sizeof_type(type, arch::i386) - sizeof_type(type, arch::x86_64);
}

/* ALIGNOF */

static size_t alignof_record(CXType type, arch a) {
   record_decl decl(type);

   if (decl.packed) {
      return 1;
   } else {
      size_t align = 1;
      for (CXType field_type : decl.field_types) {
         size_t field_align = alignof_type(field_type, a);

         // NOTE: This seems inconsistent, but it appears how clang works.
         if (field_align == 8 && a == arch::i386) {
            field_align = 4;
         }
         
         align = std::max(align, field_align);
      }
      return align;
   }
}

size_t alignof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Record:
      return alignof_record(type, a);
   case CXType_ConstantArray:
      return alignof_type(clang_getElementType(type), a);
   default:
      return std::max<size_t>(sizeof_type(type, a), 1);
   }
}

