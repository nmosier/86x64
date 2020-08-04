/* convgen.cc -- runtime to between 32-bit and 64-bit data types */

#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <sstream>

#include <getopt.h>

#include <clang-c/Index.h>

#include "util.hh"
#include "typeinfo.hh"


/* conversion from traditional type to stdint type */
void standardize_type(std::ostream& os, CXType type, arch a) {
   switch (type.kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
      
   case CXType_Char_U:
   case CXType_Char_S:
   case CXType_UChar:
   case CXType_SChar:
   case CXType_Short:
   case CXType_UShort:
   case CXType_Int:
   case CXType_UInt:
   case CXType_Float:
   case CXType_LongLong:
   case CXType_ULongLong:
   case CXType_Double:
      os << to_string(type);
      break;
      
   case CXType_Enum:
      os << "int";
      break;
      
   case CXType_Long:
   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_BlockPointer:
      switch (a) {
      case arch::i386: os << "int32_t"; break;
      case arch::x86_64: os << "int64_t"; break;
      default: abort();
      }
      break;
   case CXType_ULong:
      switch (a) {
      case arch::i386: os << "uint32_t"; break;
      case arch::x86_64: os << "uint64_t"; break;
      default: abort();
      }
      break;
      
   case CXType_ConstantArray: abort();
      
   default: abort();
   }
}


struct StringHash {
   template <typename T>
   inline std::size_t operator()(const T& struct_decl) const {
      return std::hash<std::string>()(to_string(struct_decl));
   }
};

struct StringEqual {
   template <typename T>
   inline std::size_t operator()(const T& lhs, const T& rhs) const {
      return to_string(lhs) == to_string(rhs);
   }
};

using StringSet = std::unordered_set<std::string>;
using TypeSet = std::unordered_set<CXType, StringHash, StringEqual>;
using CursorSet = std::unordered_set<CXCursor, StringHash, StringEqual>;

struct ConversionGenerator {
   CXIndex index = clang_createIndex(0, 0);

   std::ostream& os;
   std::stringstream includes;
   std::stringstream forward_decls;
   std::stringstream type_defs;
   std::stringstream function_protos;

   StringSet ignore_structs;
   CursorSet structs;
   TypeSet types;

   std::list<CXTranslationUnit> translation_units;

   ConversionGenerator(std::ostream& os, const StringSet& ignore_structs):
      os(os), ignore_structs(ignore_structs) {}

   ~ConversionGenerator() {
      for (CXTranslationUnit unit : translation_units) {
         clang_disposeTranslationUnit(unit);
      }
      clang_disposeIndex(index);
   }

   void add_file(const std::string& path) {
      CXTranslationUnit unit = clang_parseTranslationUnit(index, path.c_str(), nullptr, 0, nullptr,
                                                          0, CXTranslationUnit_None);
      if (unit == nullptr) {
         throw std::invalid_argument("unable to parse translation unit");
      }
      translation_units.push_back(unit);
   }

   inline void handle_cursor(CXCursor c, CXCursor p) {
      switch (clang_getCursorKind(c)) {
      case CXCursor_StructDecl:
         handle_struct_decl(c);
         break;
      default: break;
      }
   }

   inline void handle_struct_decl(CXCursor c) {
      if (ignore_structs.find(to_string(clang_getCursorType(c))) == ignore_structs.end()) {
         structs.insert(c);
      }
   }

   
   
};

int main(int argc, char *argv[]) {
   auto usage = [=] (FILE *f) {
                   const char *usage =
                      "usage: %s [-h] [-o <path>] [-r <structfile>] header...\n" \
                      "";
                   fprintf(stderr, usage, argv[0]);
                };

   const char *outpath = nullptr;
   const char *structpath = nullptr;
   const char *optstring = "ho:r:";
   const struct option longopts[] = {{"help", no_argument, nullptr, 'h'},
                                     {"output", required_argument, nullptr, 'o'},
                                     {0}};
   int optchar;
   while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;
      case 'o':
         outpath = optarg;
         break;
      case '?':
         usage(stderr);
         return 1;
      }
   }

   StringSet ignore_types;
   if (structpath) {
      std::ifstream f;
      f.open(structpath);
      std::string line;
      while (std::getline(f, line)) {
         ignore_types.insert(line);
      }
   }

   std::ofstream of;
   std::ostream *os;
   if (outpath) {
      of.open(outpath);
      os = &of;
   } else {
      os = &std::cout;
   }

   /* 1. Get list of all structs that require conversion.
    * 2. Generate 32-bit and 64-bit declarations and definitions of each struct.
    * 3. Get list of all types that require conversion (structs, constant arrays, 
    *    perhaps even data types.
    * 4. Generate conversion function for each.
    */
   
   
   
   return 0;
}
