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

   emit_inst(os, "push", "rcx");
   emit_inst(os, "push", "rdi");
   emit_inst(os, "push", "rsi");

   emit_inst(os, "mov", "ecx", arrlen);
   emit_inst(os, "lea", "rdi", dst.memop());
   emit_inst(os, "lea", "rsi", src.memop());

   os << loop << ":";
   {
      memloc src = {"rsi", 0};
      memloc dst = {"rdi", 0};
      convert_type(os, elem, src, dst);
      emit_inst(os, "add", "rdi", sizeof_type(elem, arch::x86_64));
      emit_inst(os, "add", "rsi", sizeof_type(elem, arch::i386));
      emit_inst(os, "dec", "ecx");
      emit_inst(os, "jnz", loop);
   }
   
   emit_inst(os, "pop", "rsi");
   emit_inst(os, "pop", "rdi");
   emit_inst(os, "pop", "rcx");

   dst += sizeof_type(elem, arch::x86_64) * arrlen;
   src += sizeof_type(elem, arch::i386) * arrlen;
}

static void convert_struct(std::ostream& os, CXType type, memloc& src, memloc& dst) {
   CXCursor decl = clang_getTypeDeclaration(type);

   for_each(decl,
            [&] (CXCursor field, CXCursor p) {
               assert(clang_getCursorKind(field) == CXCursor_FieldDecl);
               CXType field_type = clang_getCursorType(field);
               size_t size32 = sizeof_type(field_type, arch::i386);
               size_t size64 = sizeof_type(field_type, arch::x86_64);

               src.align(size32);
               dst.align(size64);

               convert_type(os, field_type, src, dst);

               return CXChildVisit_Continue;
            });

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

   default:
      abort();
   }
}

static size_t sizeof_struct(CXType type, arch a) {
   const CXCursor decl = clang_getTypeDeclaration(type);

   size_t size = 0;
   size_t align = 0;
   
   for_each(decl,
            [&] (CXCursor field, CXCursor p) {
               assert(clang_getCursorKind(field) == CXCursor_FieldDecl);
               
               const CXType field_type = clang_getCursorType(field);
               const size_t field_size = sizeof_type(field_type, a);
               const size_t field_align = alignof_type(field_type, a);

               size = align_up(size, field_align) + field_size;
               align = std::max(align, field_align);
               
               return CXChildVisit_Continue;
            });

   size = align_up(size, align);
   return size;
}

int sizeof_type_archcmp(CXType type) {
   return sizeof_type(type, arch::i386) - sizeof_type(type, arch::x86_64);
}

/* ALIGNOF */

static size_t alignof_struct(CXType type, arch a) {
   const CXCursor decl = clang_getTypeDeclaration(type);

   size_t align = 0;

   for_each(decl,
            [&] (CXCursor field, CXCursor p) {
               assert(clang_getCursorKind(field) == CXCursor_FieldDecl);

               const CXType field_type = clang_getCursorType(field);
               const size_t field_align = alignof_type(field_type, a);

               align = std::max(align, field_align);

               return CXChildVisit_Continue;
            });

   return align;
}

size_t alignof_type(CXType type, arch a) {
   switch (type.kind) {
   case CXType_Record:
      return alignof_struct(type, a);
   default:
      return sizeof_type(type, a);
   }
}
