#ifndef MACHO_H
#define MACHO_H

#include <stdio.h>

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/thread_status.h>

typedef off_t macho_off_t;
typedef uint64_t macho_addr_t;
#define MACHO_BAD_ADDR ((macho_addr_t) -1)

struct fat_archive {
   struct fat_arch header;
   void *data;
};

struct fat {
   struct fat_header header;
   struct fat_archive *archives;
};

struct archive_32 {
   struct mach_header header;
   union load_command_32 *commands;
};

struct section_wrapper_32 {
   struct section section;
   void *data;
   int32_t adiff; /*!< Difference in address during build (erased by next build) */
   int32_t odiff; /*!< Difference in offset during build (erased by next build) */
};

struct section_wrapper_64 {
   struct section_64 section;
   void *data;
   int64_t adiff; /*!< Difference in address during build (erased by next build) */
   int64_t odiff; /*!< Difference in offset during build (erased by next build) */
};

struct segment_32 {
   struct segment_command command;
   struct section_wrapper_32 *sections;
   int32_t adiff;
};

struct segment_64 {
   struct segment_command_64 command;
   struct section_wrapper_64 *sections;
   int64_t adiff;
};

struct symtab_64 {
   struct symtab_command command;
   struct nlist_64 *entries;
   char *strtab;
};

struct symtab_32 {
   struct symtab_command command;
   struct nlist *entries;
   char *strtab;
};

struct dysymtab_64 {
   struct dysymtab_command command;
   uint32_t *indirectsyms;
};

struct dysymtab_32 {
   struct dysymtab_command command;
   uint32_t *indirectsyms;
};

struct dylinker {
   struct dylinker_command command;
   char *name;
};

struct linkedit_data {
   struct linkedit_data_command command;
   void *data;
};

struct dyld_info {
   struct dyld_info_command command;
   void *rebase_data;
   void *bind_data;
   void *lazy_bind_data;
   void *export_data;
};

/** Machine-specific data structure following a thread_command. */
struct thread_header {
   uint32_t flavor;
   uint32_t count;
};

union thread_entry {
   struct thread_header header;
   struct x86_thread_state x86_thread;
};

struct thread_list {
   union thread_entry entry;
   struct thread_list *next;
};

struct thread {
   struct thread_command command;
   struct thread_list *entries;
};

struct dylib_wrapper {
   struct dylib_command command;
   char *name;
};

union load_command_32 {
   struct load_command load;
   struct segment_32 segment;
   struct symtab_32 symtab;
   struct dysymtab_32 dysymtab;
   struct dylinker dylinker;
   struct uuid_command uuid;
   struct thread thread;
   struct linkedit_data linkedit;
   struct dyld_info dyld_info;
   struct version_min_command version_min;
   struct source_version_command source_version;
   struct entry_point_command entry_point;
   struct dylib_wrapper dylib;
};

union load_command_64 {
   struct load_command load;
   struct segment_64 segment;
   struct symtab_64 symtab;
   struct dysymtab_64 dysymtab;
   struct dylinker dylinker;
   struct uuid_command uuid;
   struct thread thread;
   struct linkedit_data linkedit;
   struct dyld_info dyld_info;
   struct version_min_command version_min;
   struct source_version_command source_version;
   struct entry_point_command entry_point;
   struct dylib_wrapper dylib;
};

struct archive_64 {
   struct mach_header_64 header;
   union load_command_64 *commands;
};

union archive {
   uint32_t magic;
   struct archive_32 archive_32;
   struct archive_64 archive_64;
};

union macho {
   uint32_t magic;
   struct fat fat;
   union archive archive;
};

#endif
