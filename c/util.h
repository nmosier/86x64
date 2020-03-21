#ifndef UTIL_H
#define UTIL_H

#define AFTER(lval) ((void *) ((char *) &(lval) + sizeof(lval)))
#define STRUCT_REM(s, memb) (((char *) (&(s) + 1)) - ((char *) (&(s).memb + 1)))

int fread_exact(void *ptr, size_t size, size_t nitems, FILE *stream);

/**
 * Read bytes from file, allocating buffer.
 * @param size size of each item
 * @param number of items
 * @param stream input stream
 * @return allocated buffer filled with read bytes on success; NULL on error.
 */
void *fmread(size_t size, size_t nitems, FILE *stream);

#endif
