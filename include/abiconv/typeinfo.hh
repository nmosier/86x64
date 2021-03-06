#pragma once

#include <string>
#include <clang-c/Index.h>

enum class arch {i386, x86_64};
enum class reg_width {B, W, D, Q};
enum class fp_width {REAL4, REAL8, TBYTE};
enum class type_domain {INT, REAL};

size_t reg_width_size(reg_width width);
const char *reg_width_to_str(reg_width width);
const char *reg_width_to_sse(reg_width width);
bool get_type_signed(CXTypeKind type_kind);
type_domain get_type_domain(CXTypeKind kind);
reg_width get_type_width(CXTypeKind type_kind, arch a);

std::ostream& operator<<(std::ostream& os, const CXString& str);

bool operator<(reg_width lhs, reg_width rhs);
