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
    __bsdiff_log_error(ctx, errcode, fmt, ##__VA_ARGS__); \
    ret = errcode; \
    goto cleanup; \
  } while (0)

struct bsdiff_ctx;
void __bsdiff_log_error(struct bsdiff_ctx *ctx, int errcode, const char *fmt, ...);

#endif // !__BSDIFF_COMMON_H__