#pragma once

#include "typeinfo.hh"

struct memloc;

size_t sizeof_type(CXType type, arch a);

/* compare sizes between architectures */
int sizeof_type_archcmp(CXType type);

size_t alignof_type(CXType type, arch a);

void convert_type(std::ostream& os, CXType type, memloc& src, memloc& dst);
