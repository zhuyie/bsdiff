/** @file bsdiff_mem.h */

#ifndef __BSDIFF_MEM_H__
#define __BSDIFF_MEM_H__

#include <stddef.h>

/*
 * Internal memory allocation wrappers with tracking.
 * These should be used instead of malloc/realloc/free
 * throughout the bsdiff source code.
 */
void *bsdiff_malloc(size_t size);
void *bsdiff_realloc(void *ptr, size_t size);
void  bsdiff_free(void *ptr);

#endif /* !__BSDIFF_MEM_H__ */
