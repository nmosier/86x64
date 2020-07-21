#ifndef LEB_H
#define LEB_H

#include <stdint.h>
#include <stdlib.h>

#define ULEB128_MAXBITS (sizeof(uintmax_t) * 8)
#define ULEB128_DATAMASK 0x7f
#define ULEB128_CTRLMASK 0x80

#define SLEB128_MAXBITS (sizeof(intmax_t) * 8)
#define SLEB128_DATAMASK 0x7f
#define SLEB128_CTRLMASK 0x80
#define SLEB128_SIGNBIT  0x40

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
