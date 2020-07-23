/* symtab.hpp -- symbol table for identifier table and string table */

#ifndef __SYMTAB_HPP
#define __SYMTAB_HPP

#include <unordered_map>
#include <string>

typedef std::string Symbol;
typedef std::unordered_map<std::string, Symbol *> IdentifierTable;
typedef std::unordered_map<std::string, std::string *> StringTable;

extern StringTable g_str_tab;
extern IdentifierTable g_id_tab;

#endif
