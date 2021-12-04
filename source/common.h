#ifndef __BSDIFF_COMMON_H__
#define __BSDIFF_COMMON_H__

#include <sys/types.h>
#include <bzlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)

#include <basetsd.h>
typedef SSIZE_T ssize_t;

#endif

#define HANDLE_ERROR(errcode, fmt, ...) \
  do { \
    fprintf(stderr, "ERROR(%d): ", errcode); \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    fprintf(stderr, "\n"); \
    ret = errcode; \
    goto cleanup; \
  } while (0)

#endif // !__BSDIFF_COMMON_H__