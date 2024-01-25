/** @file bsdiff.h */

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

#ifdef BSDIFF_DLL
#  ifdef _WIN32
#    ifdef BSDIFF_EXPORTS
#      define BSDIFF_API __declspec(dllexport)
#    else
#      define BSDIFF_API __declspec(dllimport)
#    endif
#  elif __GNUC__ >= 4
#    define BSDIFF_API __attribute__ ((visibility("default")))
#  else
#    define BSDIFF_API
#  endif
#else
#  define BSDIFF_API
#endif

/* return codes */
#define BSDIFF_SUCCESS          0    /* success */
#define BSDIFF_ERROR            1    /* generic error */
#define BSDIFF_INVALID_ARG      2    /* invalid argument */
#define BSDIFF_OUT_OF_MEMORY    3    /* out of memory */
#define BSDIFF_FILE_ERROR       4    /* file related errors */
#define BSDIFF_END_OF_FILE      5    /* end of file */
#define BSDIFF_CORRUPT_PATCH    6    /* corrupt patch data */
#define BSDIFF_SIZE_TOO_LARGE   7    /* size is too large */

/* modes */
#define BSDIFF_MODE_READ  0
#define BSDIFF_MODE_WRITE 1

/* seek origins */
#define BSDIFF_SEEK_SET 0
#define BSDIFF_SEEK_CUR 1
#define BSDIFF_SEEK_END 2


/**
 * @brief Interface of a stream.
 */
struct bsdiff_stream
{
	void *state;
	/* common */
	void (*close)(void *state);
	int (*get_mode)(void *state);
	int (*seek)(void *state, int64_t offset, int origin);
	int (*tell)(void *state, int64_t *position);
	/* read mode only */
	int (*read)(void *state, void *buffer, size_t size, size_t *readed);
	/* write mode only */
	int (*write)(void *state, const void *buffer, size_t size);
	int (*flush)(void *state);
	/* optional */
	int (*get_buffer)(void *state, const void **ppbuffer, size_t *psize);
};

/**
 * @brief
 *    Open a file based bsdiff_stream.
 * @param mode
 *    The working mode of the stream.
 * @param filename
 *    The name of the file.
 * @param stream
 *    The stream to be opened.
 * @return
 *    BSDIFF_SUCCESS if no error.
 */
BSDIFF_API
int bsdiff_open_file_stream(
	int mode,
	const char *filename, 
	struct bsdiff_stream *stream);

/**
 * @brief
 *    Open a memory based bsdiff_stream.
 * @param mode
 *    The working mode of the stream.
 * @param buffer
 *    Should be a valid buffer if mode=read, otherwise it should be NULL.
 * @param size
 *    Should be the corresponding length of the buffer if mode=read,
 *    otherwise it specify the initial capacity of the stream.
 * @param stream
 *    The stream to be opened.
 * @return
 *    BSDIFF_SUCCESS if no error.
 */
BSDIFF_API
int bsdiff_open_memory_stream(
	int mode,
	const void *buffer,
	size_t size,
	struct bsdiff_stream *stream);

/**
 * @brief
 *    Close a bsdiff_stream.
 * @param stream
 *    The stream to be closed.
 */
BSDIFF_API
void bsdiff_close_stream(
	struct bsdiff_stream *stream);


/**
 * @brief Interface of a patch packer.
 * 
 * Logically a patch consists of:
 * - Meta info: the size of the new file;
 * - An array of entries, each contains a header, an optional diff data, and an optional extra data;
 *
 * Entry header is a (diff_len, extra_len, seek_len) triple.
 */
struct bsdiff_patch_packer
{
	void *state;
	/* common */
	void (*close)(void *state);
	int (*get_mode)(void *state);
	/* read mode only */
	int (*read_new_size)(
		void *state, int64_t *size);
	int (*read_entry_header)(
		void *state, int64_t *diff, int64_t *extra, int64_t *seek);
	int (*read_entry_diff)(
		void *state, void *buffer, size_t size, size_t *readed);
	int (*read_entry_extra)(
		void *state, void *buffer, size_t size, size_t *readed);
	/* write mode only */
	int (*write_new_size)(
		void *state, int64_t size);
	int (*write_entry_header)(
		void *state, int64_t diff, int64_t extra, int64_t seek);
	int (*write_entry_diff)(
		void *state, const void *buffer, size_t size);
	int (*write_entry_extra)(
		void *state, const void *buffer, size_t size);
	int (*flush)(void *state);
};

/**
 * @brief
 *    Open a bzip2 bsdiff_patch_packer.
 * @param mode
 *    The working mode of the packer.
 * @param stream
 *    The stream which managed the reading/writing of the persistent patch data.
 * @param packer
 *    The packer to be opened.
 * @return
 *    BSDIFF_SUCCESS if no error.
 */
BSDIFF_API
int bsdiff_open_bz2_patch_packer(
	int mode,
	struct bsdiff_stream *stream,
	struct bsdiff_patch_packer *packer);

/**
 * @brief
 *    Close a bsdiff_patch_packer.
 * @param packer
 *    The packer to be closed.
 */
BSDIFF_API
void bsdiff_close_patch_packer(
	struct bsdiff_patch_packer *packer);


/**
 * @brief Some user-defined callbacks.
 */
struct bsdiff_ctx
{
	void *opaque;
	void (*log_error)(void *opaque, const char *errmsg);
};

/**
 * @brief
 *    Generate a patch between two binary files.
 * @param ctx
 *    The context.
 * @param oldfile
 *    The stream of the old file.
 * @param newfile
 *    The stream of the new file.
 * @param packer
 *    The packer.
 * @return
 *    BSDIFF_SUCCESS if no error.
 */
BSDIFF_API
int bsdiff(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile, 
	struct bsdiff_patch_packer *packer);

/**
 * @brief
 *    Apply the patch to the old file, re-create the new file.
 * @param ctx
 *    The context.
 * @param oldfile
 *    The stream of the old file.
 * @param newfile
 *    The stream of the new file.
 * @param packer
 *    The packer.
 * @return
 *    BSDIFF_SUCCESS if no error.
 */
BSDIFF_API
int bspatch(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile,
	struct bsdiff_patch_packer *packer);

#ifdef __cplusplus
}
#endif

#endif /* !__BSDIFF_BSDIFF_H__ */
