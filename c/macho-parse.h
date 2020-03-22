#ifndef MACHO_H
#define MACHO_H

#include <stdio.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>

enum endian {ENDIAN_LITTLE, ENDIAN_BIG};

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
};

struct section_wrapper_64 {
   struct section_64 section;
   void *data;
};

struct segment_32 {
   struct segment_command command;
   struct section_wrapper_32 *sections;
};

struct segment_64 {
   struct segment_command_64 command;
   struct section_wrapper_64 *sections;
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
};

struct dysymtab_32 {
   struct dysymtab_command command;
};

struct dylinker {
   struct dylinker_command command;
   char *name;
};

struct linkedit_data {
   struct linkedit_data_command command;
   void *data;
};

/** Machine-specific data structure following a thread_command. */
struct thread_entry {
   uint32_t flavor;
   uint32_t count;
   uint32_t *state;
};

struct thread {
   struct thread_command command;
   uint32_t nentries;
   struct thread_entry *entries;
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


/* NOTE: Currently assumes endianness is same as machine this program is running on, 
 * i.e. it doesn't adjust endianness.
 */
int macho_parse(FILE *f, union macho *macho);

/**
 * Parse fat Mach-O file.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param fat output Mach-O fat structure
 * @return 0 on success; -1 on error.
 */
int macho_parse_fat(FILE *f, const uint32_t *magic, struct fat *fat);

/**
 * Parse Mach-O archive.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param archive output Mach-O archive structure
 * @return 0 on success; -1 on error.
 */
int macho_parse_archive(FILE *f, const uint32_t *magic, union archive *archive);

/**
 * Parse 32-bit Mach-O archive.
 * @param f input stream
 * @param magic magic value to initialize with if already read, or NULL to read value from stream
 * @param archive header output 32-bit Mach-O header
 */
int macho_parse_archive_32(FILE *f, const uint32_t *magic, struct archive_32 *archive);

/**
 * Parse 32-bit load command.
 * @param f input stream
 * @param command output load command structure
 * @param dataoff absolute file offset of data in archive
 */
int macho_parse_load_command_32(FILE *f, union load_command_32 *command);

#endif

