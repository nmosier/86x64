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

bool should_convert_type(CXType type) {
   CXType cmptype;
   switch (type.kind) {
   case CXType_Pointer:
      cmptype = clang_getPointeeType(type);
      break;
   default:
      return false;
   }
   
   return sizeof_type_archcmp(cmptype);
}

/* Get type that needs to be converted */
CXType get_convert_type(CXType type) {
   switch (type.kind) {
   case CXType_Pointer:
      return clang_getPointeeType(type);
   default: abort();
   }
}

void convert_POD(std::ostream& os, CXType type, memloc& src, memloc& dst) {
   switch (get_type_domain(type)) {
   case type_domain::INT:
      {
         emit_arg_int move(type, r11);
         move.load(os, src);
         move.store(os, dst);
      }
      break;
   case type_domain::REAL:
      {
         emit_arg_real move(type, 2 /* xmm2 */);
         move.load(os, src);
         move.store(os, dst);
      }
      break;
   default: abort();
   }
}

static void convert_constant_array(std::ostream& os, CXType array, memloc& src, memloc& dst);
static void convert_struct(std::ostream& os, CXType type, memloc& src, memloc& dst);

void convert_type(std::ostream& os, CXType type, memloc& src, memloc& dst) {
   if (clang_isPODType(type)) {
      convert_POD(os, type, src, dst);
   } else {
      switch (type.kind) {
      case CXType_Pointer:
         // TODO
         abort();
      
      case CXType_BlockPointer:
         // TODO
         abort();
      case CXType_ConstantArray:
         convert_constant_array(os, type, src, dst);
         break;
      
      case CXType_Record:
         convert_struct(os, type, src, dst);
         break;

      default: abort();
      }
   }
}

static void convert_constant_array(std::ostream& os, CXType array, memloc& src, memloc& dst) {
   assert(strcmp(dst.basereg, "rbp") == 0); /* dst requires frame pointer */
   
   const long long arrlen = clang_getArraySize(array);
   assert(arrlen >= 1);
   const CXType elem = clang_getArrayElementType(array);

   
   std::stringstream loop_ss;
   loop_ss << ".convert_" << dst.basereg << dst.index; // this should be unique w/i function
   std::string loop = loop_ss.str();
   
   /*   mov r12d, <arrlen>
    *   lea r13, [<dst>]
    *   lea r14, [<src>]
    * loop:
    *   <move>
    *   add r13, <size64>
    *   add r14, <size32>
    * entry:
    *   dec r12d
    *   jnz loop
    */

   emit_inst(os, "mov", "r12d", arrlen);
   emit_inst(os, "lea", "r13", dst.memop());
   emit_inst(os, "lea", "r14", src.memop());

   os << loop << ":";
   {
      memloc src = {"r14", 0};
      memloc dst = {"r13", 0};
      convert_type(os, elem, src, dst);
      emit_inst(os, "add", "r13", sizeof_type(elem, arch::x86_64));
      emit_inst(os, "add", "r14", sizeof_type(elem, arch::i386));
      emit_inst(os, "dec", "r12d");
      emit_inst(os, "jnz", loop);
   }

   dst += sizeof_type(elem, arch::x86_64) * arrlen;
   src += sizeof_type(elem, arch::i386) * arrlen;
}

static void convert_struct(std::ostream& os, CXType type, memloc& src, memloc& dst) {
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

