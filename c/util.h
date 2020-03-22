#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#define AFTER(lval) ((void *) ((char *) &(lval) + sizeof(lval)))
#define STRUCT_REM(s, memb) (((char *) (&(s) + 1)) - ((char *) (&(s).memb + 1)))

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

#endif
