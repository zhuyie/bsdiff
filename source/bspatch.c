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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>

#include "bsdiff.h"
#include "bsdiff_private.h"

int bspatch(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile,
	struct bsdiff_patch_packer *packer)
{
	int ret;
	size_t cb;
	int64_t oldsize, newsize;
	uint8_t *old = NULL, *new = NULL;
	int64_t oldpos, newpos;
	int64_t ctrl[3];
	int64_t i;

	assert(oldfile->get_mode(oldfile->state) == BSDIFF_MODE_READ);
	assert(newfile->get_mode(newfile->state) == BSDIFF_MODE_WRITE);
	assert(packer->get_mode(packer->state) == BSDIFF_MODE_READ);

	if ((oldfile->seek(oldfile->state, 0, BSDIFF_SEEK_END) != BSDIFF_SUCCESS) ||
		(oldfile->tell(oldfile->state, &oldsize) != BSDIFF_SUCCESS) ||
		(oldfile->seek(oldfile->state, 0, BSDIFF_SEEK_SET) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "retrieve size of oldfile");
	}
	if (oldsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "oldfile is too large");
	if ((old = malloc((size_t)(oldsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for old");
	if (oldfile->read(oldfile->state, old, (size_t)oldsize, &cb) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read oldfile");

	if (packer->read_new_size(packer->state, &newsize) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read new size from patch_packer");
	if (newsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "newfile is too large");
	if ((new = malloc((size_t)(newsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for new");

	oldpos = 0; newpos = 0;
	while (newpos < newsize) {
		/* Read control data */
		ret = packer->read_entry_header(packer->state, &ctrl[0], &ctrl[1], &ctrl[2]);
		if (ret != BSDIFF_SUCCESS && ret != BSDIFF_END_OF_FILE)
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read control data");

		/* Sanity-check */
		if ((ctrl[0] < 0) || (ctrl[1] < 0))
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid control data");
		if (newpos + ctrl[0] > newsize)
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid control data");

		/* Read diff string */
		if (ctrl[0] >= SIZE_MAX)
			HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "read diff string");
		ret = packer->read_entry_diff(packer->state, new + newpos, (size_t)ctrl[0], &cb);
		if ((ret != BSDIFF_SUCCESS && ret != BSDIFF_END_OF_FILE) || (cb != (size_t)ctrl[0]))
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read diff string");

		/* Add old data to diff string */
		for (i = 0; i < ctrl[0]; i++) {
			if ((oldpos + i >= 0) && (oldpos + i < oldsize))
				new[newpos + i] += old[oldpos + i];
		}

		/* Adjust pointers */
		newpos += ctrl[0];
		oldpos += ctrl[0];

		/* Sanity-check */
		if (newpos + ctrl[1] > newsize)
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid control data");

		/* Read extra string */
		if (ctrl[1] >= SIZE_MAX)
			HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "read extra string");
		ret = packer->read_entry_extra(packer->state, new + newpos, (size_t)ctrl[1], &cb);
		if ((ret != BSDIFF_SUCCESS && ret != BSDIFF_END_OF_FILE) || (cb != (size_t)ctrl[1]))
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read extra string");

		/* Adjust pointers */
		newpos += ctrl[1];
		oldpos += ctrl[2];
	};

	/* Write the new file */
	if ((newfile->write(newfile->state, new, (size_t)newsize) != BSDIFF_SUCCESS) ||
		(newfile->flush(newfile->state) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "write newfile");
	}

	ret = BSDIFF_SUCCESS;

cleanup:
	if (new != NULL) { free(new); }
	if (old != NULL) { free(old); }

	return ret;
}
