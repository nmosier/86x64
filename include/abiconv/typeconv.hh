#pragma once

#include <list>
#include <string>
#include <unordered_set>

#include "loc.hh"
#include "util.hh"
#include "typeinfo.hh"

using Symbols = std::unordered_set<std::string>;

struct memloc;

size_t sizeof_type(CXType type, arch a);
size_t sizeof_type(CXTypeKind type_kind, arch a);

/* compare sizes between architectures */
int sizeof_type_archcmp(CXType type);

size_t alignof_type(CXType type, arch a);

/**
 * Emit code to convert values between architectures at runtime.
 */
void convert_type(std::ostream& os, CXType type, arch a, Location& src, Location& dst,
                  Location& data);

struct struct_decl {
   using FieldTypes = std::list<CXType>;
   
   CXCursor cursor;
   FieldTypes field_types;
   bool packed = false;

   struct_decl(CXCursor cursor);
   struct_decl(CXType type);

private:
   void populate_fields();
};


class conversion {
public:
   bool allocate; /*!< whether to allocate new data or copy existing */
   arch from_arch;
   arch to_arch;
   MemoryLocation data;
   const Symbols& ignore_structs;

   std::string label() { return std::string(".") + std::to_string(label_++); }

   void convert(std::ostream& os, CXType type, const Location& src, const Location& dst);
   
   void convert_int(std::ostream& os, CXTypeKind type_kind, const Location& src,
                    const Location& dst);
   void convert_real(std::ostream& os, CXTypeKind type_kind, const Location& src,
                     const Location& dst);
   void convert_void_pointer(std::ostream& os, const Location& src,
                             const Location& dst);
   void convert_constant_array(std::ostream& os, CXType array, MemoryLocation src,
                               MemoryLocation dst);
   void convert_pointer(std::ostream& os, CXType pointee, const Location& src, const Location& dst);
   void convert_record(std::ostream& os, CXType record, MemoryLocation src, MemoryLocation dst);
   
   conversion(bool allocate, arch from_arch, arch to_arch, const MemoryLocation& data,
              const Symbols& ignore_structs):
      allocate(allocate), from_arch(from_arch), to_arch(to_arch), data(data),
      ignore_structs(ignore_structs) {}
   
private:
   unsigned label_ = 0;
   
   void push(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst); 
   void pop(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst);
   void push(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
   void pop(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
};
