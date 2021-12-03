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

#define err(eval, fmt, ...) \
  do { \
    fprintf(stderr, fmt, __VA_ARGS__); \
    fprintf(stderr, ": %s\n", strerror(errno)); \
    exit(eval); \
  } while(0)

#define errx(eval, fmt, ...) \
  do { \
    fprintf(stderr, fmt, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(eval); \
  } while(0)

#else

#include <err.h>

#endif

#endif // !__BSDIFF_COMMON_H__