#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>

#define AFTER(lval) ((void *) ((char *) &(lval) + sizeof(lval)))
#define STRUCT_REM(s, memb) (((char *) (&(s) + 1)) - ((char *) (&(s).memb + 1)))

#define ALIGN_UP(n, align) ((((n) + (align) - 1) / (align)) * (align))
#define ALIGN_DOWN(n, align) (((n) / (align)) * (align))

#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define PAGESIZE 0x1000

#define STR(s) #s
#define XSTR(s) STR(s)

int fread_exact(void *ptr, size_t size, size_t nitems, FILE *stream);
int fwrite_exact(const void *ptr, size_t size, size_t nitems, FILE *stream);

/**
 * Read bytes from file, allocating buffer.
 * @param size size of each item
 * @param number of items
 * @param stream input stream
 * @return allocated buffer filled with read bytes on success; NULL on error.
 */
void *fmread(size_t size, size_t nitems, FILE *stream);

/**
 * Write bytes at given offset.
 * @param ptr buffer to write
 * @param size size of each item
 * @param nitems number of items
 * @param stream output file
 * @param offset offset to write at, relative to beginning of file (SEEK_SET)
 */
size_t fwrite_at(const void *ptr, size_t size, size_t nitems, FILE *stream, off_t offset);

size_t fread_at(void *ptr, size_t size, size_t nitems, FILE *stream, off_t offset);

/**
 * Merge a collection of arrays of items into one array.
 * @param collection collection of arrays
 * @param counts counts of elements in each array
 * @param narrs number of arrays in collection
 * @param itemsize size of each item in arrays
 * @param merged output pointer to merged array
 * @return number of elements in merged array 
 */
ssize_t merge(const void *collection[], const size_t counts[], size_t narrs, size_t itemsize,
              void **merged);

void *fmread_at(size_t size, size_t nitems, FILE *stream, off_t offset);

/**
 * Duplicate memory.
 * @param buffer to duplicate
 * @param size of buffer to duplicate
 * @return duplicate of buffer on success; NULL on error.
 */
void *memdup(const void *buf, size_t size);

/**************
 * LEB128 API *
 **************/

#include <stdint.h>

#define ULEB128_MAXBITS (sizeof(uintmax_t) * 8)
#define ULEB128_DATAMASK 0x7f
#define ULEB128_CTRLMASK 0x80

#define SLEB128_MAXBITS (sizeof(intmax_t) * 8)
#define SLEB128_DATAMASK 0x7f
#define SLEB128_CTRLMASK 0x80
#define SLEB128_SIGNBIT  0x40

/**
 * Decode unsigned LEB128 value from buffer.
 * @param buf input buffer
 * @param buflen length of input buffer
 * @param n pointer to where to place result. Can be NULL.
 * @return number of bytes used; if exceeds buflen, then need more bytes; if zero, then overflow.
 */
size_t uleb128_decode(const void *buf, size_t buflen, uintmax_t *n);

/**
 * Encode unsigend LEB128 value to buffer.
 * @param buf output buffer, or NULL to do a dry run
 * @param buflen length of buffer
 * @param n value to encode
 * @return number of bytes written; if 0, then need more buffer space.
 */
size_t uleb128_encode(void *buf, size_t buflen, uintmax_t n);

/**
 * Decode a signed LEB128 value from buffer.
 * @param buf input buffer
 * @param buflen length of input buffer
 * @param n pointer to result, if non-NULL
 * @return number of bytes used; if exceeds buflen, then need more bytes in buffer; if zero, 
 *         then overflow.
 */
size_t sleb128_decode(const void *buf, size_t buflen, intmax_t *n);

size_t sleb128_encode(void *buf, size_t buflen, intmax_t n);

#endif
