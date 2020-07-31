#pragma once

#include <list>

#include "loc.hh"
#include "util.hh"
#include "typeinfo.hh"

struct memloc;

size_t sizeof_type(CXType type, arch a);

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

   void convert(std::ostream& os, CXType type, const Location& src, const Location& dst);
   
   void convert_int(std::ostream& os, CXType type, const Location& src, const Location& dst);
   void convert_real(std::ostream& os, CXType type, const Location& src, const Location& dst);
   void convert_void_pointer(std::ostream& os, CXType type, const Location& src,
                             const Location& dst);
private:
   void push(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst); 
   void pop(std::ostream& os, const RegisterLocation& loc, Location& src, Location& dst);
   void push(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
   void pop(std::ostream& os, const SSELocation& loc, Location& src, Location& dst);
};
