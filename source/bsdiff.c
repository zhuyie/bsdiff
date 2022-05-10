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

#include <bzlib.h>
#include <divsufsort.h>
#include <divsufsort64.h>

#include "bsdiff.h"
#include "bsdiff_private.h"

#define DB_BUF_LEN 65536
#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static int64_t matchlen(uint8_t *old, int64_t oldsize, uint8_t *new, int64_t newsize)
{
	int64_t i;

	for (i = 0; (i < oldsize) && (i < newsize); i++) {
		if (old[i] != new[i])
			break;
	}
	return i;
}

static int64_t search32(uint8_t *buf, uint8_t *old, int64_t oldsize,
		uint8_t *new, int64_t newsize, int64_t st, int64_t en, int64_t *pos)
{
	int64_t x, y;
	int32_t *SA = (int32_t*)buf;

	if (en-st < 2) {
		x = matchlen(old+SA[st], oldsize-SA[st], new, newsize);
		y = matchlen(old+SA[en], oldsize-SA[en], new, newsize);

		if (x > y) {
			*pos = SA[st];
			return x;
		} else {
			*pos = SA[en];
			return y;
		}
	};

	x = st+(en-st)/2;
	if (memcmp(old+SA[x], new, (size_t)MIN(oldsize-SA[x], newsize)) < 0) {
		return search32(buf, old, oldsize, new, newsize, x, en, pos);
	} else {
		return search32(buf, old, oldsize, new, newsize, st, x, pos);
	};
}

static int64_t search64(uint8_t *buf, uint8_t *old, int64_t oldsize,
		uint8_t *new, int64_t newsize, int64_t st, int64_t en, int64_t *pos)
{
	int64_t x, y;
	int64_t *SA = (int64_t*)buf;

	if (en-st < 2) {
		x = matchlen(old+SA[st], oldsize-SA[st], new, newsize);
		y = matchlen(old+SA[en], oldsize-SA[en], new, newsize);

		if (x > y) {
			*pos = SA[st];
			return x;
		} else {
			*pos = SA[en];
			return y;
		}
	};

	x = st+(en-st)/2;
	if (memcmp(old+SA[x], new, (size_t)MIN(oldsize-SA[x], newsize)) < 0) {
		return search64(buf, old, oldsize, new, newsize, x, en, pos);
	} else {
		return search64(buf, old, oldsize, new, newsize, st, x, pos);
	};
}

int bsdiff(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile, 
	struct bsdiff_patch_packer *packer)
{
	int ret;
	uint8_t *old = NULL, *new = NULL;
	int64_t oldsize, newsize;
	int64_t scan, pos, len;
	int64_t lastscan, lastpos, lastoffset;
	int64_t oldscore, scsc;
	int64_t s, Sf, lenf, Sb, lenb;
	int64_t overlap, Ss, lens;
	int64_t i, j;
	int64_t dblen;
	uint8_t *db = NULL;
	size_t cb;
	int64_t (*psearch)(uint8_t*, uint8_t*, int64_t, uint8_t*, 
		int64_t, int64_t, int64_t, int64_t*);
	int64_t bufsize;
	uint8_t *SA = NULL;

	assert(oldfile->get_mode(oldfile->state) == BSDIFF_MODE_READ);
	assert(newfile->get_mode(newfile->state) == BSDIFF_MODE_READ);
	assert(packer->get_mode(packer->state) == BSDIFF_MODE_WRITE);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
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

	/* Construct the suffix array */
	bufsize = (oldsize + 1) * sizeof(int64_t);
	if (oldsize < 0x7fffffff)
		bufsize /= 2;
	if (bufsize < SIZE_MAX)
		SA = malloc((size_t)bufsize);
	if (SA == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for SA");

	if (oldsize < 0x7fffffff)
	{
		((int32_t*)SA)[0] = (int32_t)oldsize;
		if (divsufsort(old, ((int32_t*)SA) + 1, (int32_t)oldsize) != 0)
			HANDLE_ERROR(BSDIFF_ERROR, "construct suffix array");
		psearch = search32;
	}
	else
	{
		((int64_t*)SA)[0] = (int64_t)oldsize;
		if (divsufsort64(old, ((int64_t*)SA) + 1, (int64_t)oldsize) != 0)
			HANDLE_ERROR(BSDIFF_ERROR, "construct suffix array");
		psearch = search64;
	}

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if ((newfile->seek(newfile->state, 0, BSDIFF_SEEK_END) != BSDIFF_SUCCESS) ||
		(newfile->tell(newfile->state, &newsize) != BSDIFF_SUCCESS) ||
		(newfile->seek(newfile->state, 0, BSDIFF_SEEK_SET) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "retrieve size of newfile");
	}
	if (newsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "newfile is too large");
	if ((new = malloc((size_t)(newsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for new");
	if (newfile->read(newfile->state, new, (size_t)newsize, &cb) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read newfile");

	if ((db = malloc(DB_BUF_LEN)) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for db");

	/* Begin write */
	if (packer->write_new_size(packer->state, newsize) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "write new size");

	/* Scan */
	scan = 0; len = 0;
	lastscan = 0; lastpos = 0; lastoffset = 0;
	while (scan < newsize) {
		oldscore = 0;

		for (scsc = scan+=len; scan < newsize; scan++) {
			len = psearch(SA, old, oldsize, new+scan, newsize-scan,
					0, oldsize, &pos);

			for (; scsc < scan + len; scsc++) {
				if ((scsc + lastoffset < oldsize) &&
					(old[scsc + lastoffset] == new[scsc]))
				{
					oldscore++;
				}
			}

			if (((len == oldscore) && (len != 0)) ||
				(len > oldscore + 8))
			{
				break;
			}

			if ((scan + lastoffset < oldsize) &&
				(old[scan + lastoffset] == new[scan]))
			{
				oldscore--;
			}
		};

		if ((len != oldscore) || (scan == newsize)) {
			s = 0; Sf = 0; lenf = 0;
			for (i = 0; (lastscan+i<scan) && (lastpos+i<oldsize);) {
				if (old[lastpos+i] == new[lastscan+i])
					s++;
				i++;
				if (s*2-i > Sf*2-lenf) { 
					Sf = s; 
					lenf = i;
				};
			};

			lenb = 0;
			if (scan < newsize) {
				s = 0; Sb = 0;
				for (i = 1; (scan>=lastscan+i) && (pos>=i); i++) {
					if (old[pos-i] == new[scan-i])
						s++;
					if (s*2-i > Sb*2-lenb) {
						Sb = s; 
						lenb = i;
					};
				};
			};

			if (lastscan+lenf > scan-lenb) {
				overlap = (lastscan+lenf) - (scan-lenb);
				s = 0; Ss = 0; lens = 0;
				for (i = 0; i < overlap; i++) {
					if (new[lastscan + lenf - overlap + i] ==
						old[lastpos + lenf - overlap + i])
					{
						s++;
					}
					if (new[scan - lenb + i] ==
						old[pos - lenb + i])
					{
						s--;
					}
					if (s > Ss) {
						Ss = s; 
						lens = i+1;
					};
				};

				lenf += lens-overlap;
				lenb -= lens;
			};

			/* Write entry header */
			ret = packer->write_entry_header(
				packer->state, 
				lenf, 
				(scan-lenb)-(lastscan+lenf), 
				(pos-lenb)-(lastpos+lenf));
			if (ret != BSDIFF_SUCCESS)
				HANDLE_ERROR(BSDIFF_ERROR, "write entry header");

			/* Write entry diff */
			for (i = 0; i < lenf; ) {
				dblen = lenf - i;
				if (dblen > DB_BUF_LEN)
					dblen = DB_BUF_LEN;
				for (j = 0; j < dblen; j++) {
					db[j] = new[lastscan+i+j]-old[lastpos+i+j];
				}
				ret = packer->write_entry_diff(packer->state, db, (size_t)dblen);
				if (ret != BSDIFF_SUCCESS)
					HANDLE_ERROR(BSDIFF_ERROR, "write entry diff");
				i += dblen;
			}

			/* Write entry extra */
			if ((scan-lenb)-(lastscan+lenf) > 0) {
				ret = packer->write_entry_extra(
					packer->state, &new[lastscan+lenf], (size_t)((scan-lenb)-(lastscan+lenf)));
				if (ret != BSDIFF_SUCCESS)
					HANDLE_ERROR(BSDIFF_ERROR, "write entry extra");
			}

			lastscan = scan - lenb;
			lastpos = pos - lenb;
			lastoffset = pos - scan;
		};
	};

	/* Flush */
	if (packer->flush(packer->state) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "flush patch_packer");

	ret = BSDIFF_SUCCESS;

cleanup:
	if (db != NULL) { free(db); }
	if (SA != NULL) { free(SA); }
	if (old != NULL) { free(old); }
	if (new != NULL) { free(new); }

	return ret;
}
