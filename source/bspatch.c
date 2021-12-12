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
#include <stdint.h>
#include <sys/types.h>

#include "bsdiff.h"
#include "misc.h"
#include "sub_stream.h"

static off_t offtin(uint8_t *buf)
{
	off_t y;

	y = buf[7] & 0x7F;
	y = y * 256; y += buf[6];
	y = y * 256; y += buf[5];
	y = y * 256; y += buf[4];
	y = y * 256; y += buf[3];
	y = y * 256; y += buf[2];
	y = y * 256; y += buf[1];
	y = y * 256; y += buf[0];

	if (buf[7] & 0x80)
		y = -y;

	return y;
}

int bspatch(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *patchfile, 
	struct bsdiff_stream *newfile)
{
	int ret;
	struct bsdiff_stream cpf = { 0 }, dpf = { 0 }, epf = { 0 };
	struct bsdiff_decompressor cpfbz2 = { 0 }, dpfbz2 = { 0 }, epfbz2 = { 0 };
	int64_t read_start, read_end;
	size_t cb;
	int64_t oldsize, newsize;
	ssize_t bzctrllen, bzdatalen;
	uint8_t header[32], buf[8];
	uint8_t *old = NULL, *new = NULL;
	off_t oldpos, newpos;
	off_t ctrl[3];
	off_t i;

	/*
	File format:
		0	8	"BSDIFF40"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	ret = patchfile->read(patchfile->state, header, 32, &cb);
	if (ret == BSDIFF_END_OF_FILE)
		HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "header too short");
	else if (ret != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read patchfile");

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "magic mismatch");

	/* Read lengths from header */
	bzctrllen = offtin(header + 8);
	bzdatalen = offtin(header + 16);
	newsize = offtin(header + 24);
	if ((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0))
		HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid lengths");

	/* Open substreams and create decompressors */
	/* control block */
	read_start = 32;
	read_end = read_start + bzctrllen;
	if (bsdiff_open_substream(patchfile, read_start, read_end, &cpf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "open substream for control block");
	if (bsdiff_create_bz2_decompressor(&cpfbz2) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "create decompressor for control block");
	if (cpfbz2.init(cpfbz2.state, &cpf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "init decompressor for control block");
	/* data block */
	read_start = read_end;
	read_end = read_start + bzdatalen;
	if (bsdiff_open_substream(patchfile, read_start, read_end, &dpf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "open substream for data block");
	if (bsdiff_create_bz2_decompressor(&dpfbz2) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "create decompressor for data block");
	if (dpfbz2.init(dpfbz2.state, &dpf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "init decompressor for data block");
	/* extra block */
	read_start = read_end;
	if ((patchfile->seek(patchfile->state, 0, SEEK_END) != BSDIFF_SUCCESS) ||
		(patchfile->tell(patchfile->state, &read_end) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "get the size of patchfile");
	}
	if (bsdiff_open_substream(patchfile, read_start, read_end, &epf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "open substream for extra block");
	if (bsdiff_create_bz2_decompressor(&epfbz2) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "create decompressor for extra block");
	if (epfbz2.init(epfbz2.state, &epf) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "init decompressor for extra block");

	if ((oldfile->seek(oldfile->state, 0, SEEK_END) != BSDIFF_SUCCESS) ||
		(oldfile->tell(oldfile->state, &oldsize) != BSDIFF_SUCCESS) ||
		(oldfile->seek(oldfile->state, 0, SEEK_SET) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "get the size of oldfile");
	}
	if (oldsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "the oldfile is too large");
	if ((old = malloc((size_t)(oldsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc(old)");
	if ((oldfile->read(oldfile->state, old, oldsize, &cb) != BSDIFF_SUCCESS) ||
		(cb != oldsize))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read oldfile");
	}

	if (newsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "the newfile is too large");
	if ((new = malloc((size_t)(newsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc(new)");

	oldpos = 0; newpos = 0;
	while (newpos < newsize) {
		/* Read control data */
		for (i = 0; i <= 2; i++) {
			if (cpfbz2.read(cpfbz2.state, buf, 8, &cb) != BSDIFF_SUCCESS) {
				HANDLE_ERROR(BSDIFF_FILE_ERROR, "read control data");
			}
			ctrl[i] = offtin(buf);
		};

		/* Sanity-check */
		if (newpos + ctrl[0] > newsize)
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid control data");

		/* Read diff string */
		if (dpfbz2.read(dpfbz2.state, new + newpos, ctrl[0], &cb) != BSDIFF_SUCCESS)
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
		if (epfbz2.read(epfbz2.state, new + newpos, ctrl[1], &cb) != BSDIFF_SUCCESS)
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read extra string");

		/* Adjust pointers */
		newpos += ctrl[1];
		oldpos += ctrl[2];
	};

	/* Write the new file */
	if ((newfile->write(newfile->state, new, newsize, &cb) != BSDIFF_SUCCESS) ||
		(newfile->flush(newfile->state) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "write newfile");
	}

	ret = BSDIFF_SUCCESS;

cleanup:
	if (cpfbz2.close != NULL) { cpfbz2.close(cpfbz2.state); }
	if (dpfbz2.close != NULL) { dpfbz2.close(dpfbz2.state); }
	if (epfbz2.close != NULL) { epfbz2.close(epfbz2.state); }
	if (cpf.close != NULL) { cpf.close(cpf.state); }
	if (dpf.close != NULL) { dpf.close(dpf.state); }
	if (epf.close != NULL) { epf.close(epf.state); }
	if (new != NULL) { free(new); }
	if (old != NULL) { free(old); }

	return ret;
}
