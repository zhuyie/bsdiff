/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2021 zhuyie
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BSDIFF_BSDIFF_H__
#define __BSDIFF_BSDIFF_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* return codes */
#define BSDIFF_SUCCESS 0
#define BSDIFF_ERROR 1                  /* generic error */
#define BSDIFF_INVALID_ARG 2            /* invalid argument */
#define BSDIFF_OUT_OF_MEMORY 3          /* out of memory */
#define BSDIFF_FILE_ERROR 4             /* file related errors */
#define BSDIFF_END_OF_FILE 5            /* end of file */
#define BSDIFF_CORRUPT_PATCH 6          /* corrupt patch data */
#define BSDIFF_SIZE_TOO_LARGE 7         /* size is too large */

/* bsdiff_stream */
struct bsdiff_stream
{
	void *state;
	int (*seek)(void *state, int64_t offset, int origin);
	int (*tell)(void *state, int64_t *position);
	int (*read)(void *state, void *buffer, size_t size, size_t *readed);
	int (*write)(void *state, const void *buffer, size_t size);
	int (*flush)(void *state);
	void (*close)(void *state);
};

int bsdiff_open_file_stream(
	const char *filename, 
	int write,
	struct bsdiff_stream *stream);

/* bsdiff_compressor */
struct bsdiff_compressor
{
	void *state;
	int (*init)(void *state, struct bsdiff_stream *stream);
	int (*write)(void *state, const void *buffer, size_t size);
	int (*flush)(void *state);
	void (*close)(void *state);
};

int bsdiff_create_bz2_compressor(
	struct bsdiff_compressor *enc);

/* bsdiff_decompressor */
struct bsdiff_decompressor
{
	void *state;
	int (*init)(void *state, struct bsdiff_stream *stream);
	int (*read)(void *state, void *buffer, size_t size, size_t *readed);
	void (*close)(void *state);
};

int bsdiff_create_bz2_decompressor(
	struct bsdiff_decompressor *dec);

/* bsdiff_ctx */
struct bsdiff_ctx
{
	void *opaque;
	void (*log_error)(void *opaque, const char *errmsg);
};

int bsdiff(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile, 
	struct bsdiff_stream *patchfile);

int bspatch(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *patchfile, 
	struct bsdiff_stream *newfile);

#ifdef __cplusplus
}
#endif

#endif /* !__BSDIFF_BSDIFF_H__ */
