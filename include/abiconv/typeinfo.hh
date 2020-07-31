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
bool get_type_signed(CXType type);
type_domain get_type_domain(CXType type);
reg_width get_type_width(CXType type, arch a);

std::string to_string(CXType type);
std::string to_string(CXCursorKind kind);
std::string to_string(CXCursor cursor);
std::string to_string(CXTypeKind kind);

std::ostream& operator<<(std::ostream& os, const CXString& str);

bool operator<(reg_width lhs, reg_width rhs);
