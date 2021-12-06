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

#include "bsdiff.h"
#include "common.h"

static off_t offtin(u_char *buf)
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
	const char *oldfile, 
	const char *patchfile, 
	const char *newfile)
{
	int ret;
	FILE *f = NULL, *cpf = NULL, *dpf = NULL, *epf = NULL;
	BZFILE *cpfbz2 = NULL, *dpfbz2 = NULL, *epfbz2 = NULL;
	int cbz2err, dbz2err, ebz2err;
	ssize_t oldsize, newsize;
	ssize_t bzctrllen, bzdatalen;
	u_char header[32], buf[8];
	u_char *old = NULL, *new = NULL;
	off_t oldpos, newpos;
	off_t ctrl[3];
	off_t lenread;
	off_t i;

	/* Open patch file */
	if ((f = fopen(patchfile, "rb")) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s)", patchfile);

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
	if (fread(header, 1, 32, f) < 32) {
		if (feof(f))
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "header too short");
		else
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "fread(%s)", patchfile);
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "magic mismatch");

	/* Read lengths from header */
	bzctrllen = offtin(header + 8);
	bzdatalen = offtin(header + 16);
	newsize = offtin(header + 24);
	if ((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0))
		HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid lengths");

	/* Close patch file and re-open it via libbzip2 at the right places */
	fclose(f);
	f = NULL;
	if ((cpf = fopen(patchfile, "rb")) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s)", patchfile);
	if (fseek(cpf, 32, SEEK_SET))
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fseek(%s, %lld)", patchfile, (long long)32);
	if ((cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0)) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "BZ2_bzReadOpen(cpfbz2), bz2err(%d)", cbz2err);
	if ((dpf = fopen(patchfile, "rb")) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s)", patchfile);
	if (fseek(dpf, 32 + bzctrllen, SEEK_SET))
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fseek(%s, %lld)", patchfile, (long long)32 + bzctrllen);
	if ((dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0)) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "BZ2_bzReadOpen(dpfbz2), bz2err(%d)", dbz2err);
	if ((epf = fopen(patchfile, "rb")) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s)", patchfile);
	if (fseek(epf, 32 + bzctrllen + bzdatalen, SEEK_SET))
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fseek(%s, %lld)", patchfile, (long long)32 + bzctrllen + bzdatalen);
	if ((epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0)) == NULL)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "BZ2_bzReadOpen(epfbz2), bz2err(%d)", ebz2err);

	if (((f = fopen(oldfile, "rb")) == NULL) ||
		(fseek(f, 0, SEEK_END) != 0) ||
		((oldsize = ftell(f)) == -1) ||
		(fseek(f, 0, SEEK_SET) != 0))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s", oldfile);
	}
	if ((old = malloc(oldsize + 1)) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc(old)");
	if (fread(old, 1, oldsize, f) != oldsize)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fread(%s)", oldfile);
	fclose(f);
	f = NULL;

	if ((new = malloc(newsize + 1)) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc(new)");

	oldpos = 0; newpos = 0;
	while (newpos < newsize) {
		/* Read control data */
		for (i = 0; i <= 2; i++) {
			lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) ||
				((cbz2err != BZ_OK) && (cbz2err != BZ_STREAM_END)))
			{
				HANDLE_ERROR(BSDIFF_FILE_ERROR, "read control data");
			}
			ctrl[i] = offtin(buf);
		};

		/* Sanity-check */
		if (newpos + ctrl[0] > newsize)
			HANDLE_ERROR(BSDIFF_CORRUPT_PATCH, "invalid control data");

		/* Read diff string */
		lenread = BZ2_bzRead(&dbz2err, dpfbz2, new + newpos, ctrl[0]);
		if ((lenread < ctrl[0]) ||
		    ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END)))
		{
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read diff string");
		}

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
		lenread = BZ2_bzRead(&ebz2err, epfbz2, new + newpos, ctrl[1]);
		if ((lenread < ctrl[1]) ||
			((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END)))
		{
			HANDLE_ERROR(BSDIFF_FILE_ERROR, "read extra string");
		}

		/* Adjust pointers */
		newpos += ctrl[1];
		oldpos += ctrl[2];
	};

	/* Write the new file */
	if (((f = fopen(newfile, "wb")) == NULL) ||
		(fwrite(new, 1, newsize, f) != newsize))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "fopen(%s)", newfile);
	}

	ret = BSDIFF_SUCCESS;

cleanup:
	if (cpfbz2 != NULL) { BZ2_bzReadClose(&cbz2err, cpfbz2); }
	if (dpfbz2 != NULL) { BZ2_bzReadClose(&dbz2err, dpfbz2); }
	if (epfbz2 != NULL) { BZ2_bzReadClose(&ebz2err, epfbz2); }
	if (cpf != NULL) { fclose(cpf); }
	if (dpf != NULL) { fclose(dpf); }
	if (epf != NULL) { fclose(epf); }
	if (f != NULL) { fclose(f); }
	if (new != NULL) { free(new); }
	if (old != NULL) { free(old); }

	return ret;
}
