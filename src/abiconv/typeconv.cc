#include <clang-c/Index.h>

#include "util.hh"
#include "typeinfo.hh"
#include "typeconv.hh"
#include "abigen.hh"

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

struct_decl::struct_decl(CXCursor cursor): cursor(cursor) {
   populate_fields();
}

struct_decl::struct_decl(CXType type): cursor(clang_getTypeDeclaration(type)) {
   populate_fields();
}

void struct_decl::populate_fields() {
   for_each(cursor,
            [&] (CXCursor c, CXCursor p) {
               switch (clang_getCursorKind(c)) {
               case CXCursor_FieldDecl:
                  {
                     CXType type = clang_getCanonicalType(clang_getCursorType(c));
                     switch (type.kind) {
                     case CXType_Typedef:
                        break;
                     default:
                        field_types.push_back(type);
                        break;
                     }
                  }
                  break;

               case CXCursor_PackedAttr:
                  packed = true;
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

void conversion::convert_int(std::ostream& os, CXType type, const Location& src,
                             const Location& dst) {
   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      const RegisterLocation tmp_reg(rax);
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_reg, tmp_src, tmp_dst);
      convert_int(os, type, tmp_src, tmp_reg);
      convert_int(os, type, tmp_reg, tmp_dst);
      pop(os, tmp_reg, tmp_src, tmp_dst);
   } else {
      
      const char *opcode = nullptr;
      reg_width from_width = get_type_width(type, from_arch);
      reg_width to_width = get_type_width(type, to_arch);
      
      if (to_width == from_width) {
         opcode = "mov";
      } else {
         if (get_type_signed(type)) {
            // signed
            opcode = "movsx";
         } else {
            // unsigned
            opcode = "mov";
            if (to_width == reg_width::Q && dst.kind() == Location::Kind::MEM) {
               from_width = to_width;
            } else {
               to_width = from_width;
            }
         }
      }

      emit_inst(os, opcode, dst.op(to_width), src.op(from_width));
   }
}

void conversion::convert_real(std::ostream& os, CXType type, const Location& src,
                              const Location& dst) {

   if (src.kind() == Location::Kind::MEM && dst.kind() == Location::Kind::MEM) {
      SSELocation tmp_loc = {0};
      MemoryLocation tmp_src = dynamic_cast<const MemoryLocation&>(src);
      MemoryLocation tmp_dst = dynamic_cast<const MemoryLocation&>(dst);
      push(os, tmp_loc, tmp_src, tmp_dst);
      convert_real(os, type, src, tmp_loc);
      convert_real(os, type, tmp_loc, dst);
      pop(os, tmp_loc, tmp_src, tmp_dst);
   } else { 
      const reg_width to_width = get_type_width(type, to_arch);
      const reg_width from_width = get_type_width(type, from_arch);
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
   const CXType dummy_ptr = {CXType_Pointer, {0}};
   convert_int(os, dummy_ptr, src, dst);
}

void conversion::convert(std::ostream& os, CXType type, const Location& src, const Location& dst) {
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
      convert_int(os, type, src, dst);
      break;
      
   case CXType_Float:
   case CXType_Double:
   case CXType_LongDouble:
      convert_real(os, type, src, dst);
      return;
      
   case CXType_Pointer:
      // TODO
      convert_int(os, type, src, dst);
      break;
      
   case CXType_BlockPointer:
      // TODO
      convert_int(os, type, src, dst);
      break;

   case CXType_ConstantArray:
      convert_constant_array(os, type,
                             dynamic_cast<const MemoryLocation&>(src),
                             dynamic_cast<const MemoryLocation&>(dst));
      break;
      
   case CXType_Record:
      abort();
      
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

#if 0
static void convert_struct(std::ostream& os, CXType type, arch a, memloc& src, memloc& dst) {
   struct_decl decl(type);

   for (CXType field_type : decl.field_types) {
      size_t size32 = sizeof_type(field_type, arch::i386);
      size_t size64 = sizeof_type(field_type, arch::x86_64);
      src.align(size32);
      dst.align(size64);
      convert_type(os, field_type, src, dst);
   }

   src.align(alignof_type(type, arch::i386));
   dst.align(alignof_type(type, arch::x86_64));
}

static void convert_pointer(std::ostream& os, CXType pointer, arch a, memloc& src, memloc& dst) {
   emit_arg_int ptr(pointer, rax);

   if (should_convert_type(pointer)) {
      convert_type(os, get_convert_type(pointer), src, dst);
   } else {
      ptr.load(os, src);
      ptr.store(os, dst);
   }
}

#endif

/* SIZEOF */

static size_t sizeof_struct(CXType type, arch a);

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
      return sizeof_struct(type, a);

   case CXType_FunctionProto:
      // TODO: May need to address this case in the future.
      return 0;

   case CXType_Void:
      return 0;

   case CXType_IncompleteArray:
      abort();
      
   default:
      abort();
   }
}

static size_t sizeof_struct(CXType type, arch a) {
   struct_decl decl(type);
   
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

static size_t alignof_struct(CXType type, arch a) {
   struct_decl decl(type);

   if (decl.packed) {
      return 1;
   } else {
      size_t align = 1;
      for (CXType field_type : decl.field_types) {
         const size_t field_align = alignof_type(field_type, a);
         align = std::max(align, field_align);
      }
      return align;
   }
}

size_t alignof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Record:
      return alignof_struct(type, a);
   case CXType_ConstantArray:
      return alignof_type(clang_getElementType(type), a);
   default:
      return sizeof_type(type, a);
   }
}

