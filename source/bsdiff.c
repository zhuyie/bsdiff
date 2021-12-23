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
#include <bzlib.h>

#include "bsdiff.h"
#include "misc.h"

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(int64_t *I, int64_t *V, int64_t start, int64_t len, int64_t h)
{
	int64_t i, j, k, x, tmp, jj, kk;

	if (len < 16) {
		for (k = start; k < start + len; k += j) {
			j = 1; x = V[I[k] + h];
			for (i = 1; k + i < start + len; i++) {
				if (V[I[k+i] + h] < x) {
					x = V[I[k+i] + h];
					j = 0;
				};
				if (V[I[k+i] + h] == x) {
					tmp = I[k+j];
					I[k+j] = I[k+i];
					I[k+i] = tmp;
					j++;
				};
			};
			for (i = 0; i < j; i++)
				V[I[k+i]] = k+j-1;
			if (j == 1)
				I[k] = -1;
		};
		return;
	};

	x = V[I[start+len/2]+h];
	jj=0; kk=0;
	for (i = start; i < start+len; i++) {
		if (V[I[i]+h] < x)
			jj++;
		if (V[I[i]+h] == x)
			kk++;
	}
	jj += start;
	kk += jj;

	i = start; j=0; k=0;
	while (i < jj) {
		if (V[I[i]+h] < x) {
			i++;
		} else if(V[I[i]+h] == x) {
			tmp = I[i];
			I[i] = I[jj+j];
			I[jj+j] = tmp;
			j++;
		} else {
			tmp = I[i];
			I[i] = I[kk+k];
			I[kk+k] = tmp;
			k++;
		};
	};

	while (jj+j<kk) {
		if (V[I[jj+j]+h] == x) {
			j++;
		} else {
			tmp = I[jj+j];
			I[jj+j] = I[kk+k];
			I[kk+k] = tmp;
			k++;
		};
	};

	if (jj > start)
		split(I, V, start, jj-start, h);

	for (i = 0; i < kk-jj; i++)
		V[I[jj+i]] = kk-1;
	if (jj == kk-1)
		I[jj] = -1;

	if (start+len > kk)
		split(I, V, kk, start+len-kk, h);
}

static void qsufsort(int64_t *I, int64_t *V, uint8_t *old, int64_t oldsize)
{
	int64_t buckets[256];
	int64_t i,h,len;

	for (i = 0; i < 256; i++)
		buckets[i] = 0;
	for (i = 0; i < oldsize; i++)
		buckets[old[i]]++;
	for (i = 1; i < 256; i++)
		buckets[i] += buckets[i-1];
	for (i = 255; i > 0; i--)
		buckets[i] = buckets[i-1];
	buckets[0] = 0;

	for (i = 0; i < oldsize; i++)
		I[++buckets[old[i]]] = i;
	I[0] = oldsize;
	for (i = 0; i < oldsize; i++)
		V[i] = buckets[old[i]];
	V[oldsize] = 0;
	for (i = 1; i < 256; i++) {
		if (buckets[i] == buckets[i - 1] + 1)
			I[buckets[i]] = -1;
	}
	I[0] = -1;

	for (h = 1; I[0] != -(oldsize+1); h += h) {
		len = 0;
		for (i = 0; i < oldsize+1;) {
			if (I[i] < 0) {
				len -= I[i];
				i -= I[i];
			} else {
				if (len)
					I[i-len] = -len;
				len = V[I[i]]+1 - i;
				split(I, V, i, len, h);
				i += len;
				len = 0;
			};
		};
		if (len)
			I[i-len] = -len;
	};

	for (i = 0; i < oldsize+1; i++)
		I[V[i]] = i;
}

static int64_t matchlen(uint8_t *old, int64_t oldsize, uint8_t *new, int64_t newsize)
{
	int64_t i;

	for (i = 0; (i < oldsize) && (i < newsize); i++) {
		if (old[i] != new[i])
			break;
	}
	return i;
}

static int64_t search(int64_t *I, uint8_t *old, int64_t oldsize,
		uint8_t *new, int64_t newsize, int64_t st, int64_t en, int64_t *pos)
{
	int64_t x, y;

	if (en-st < 2) {
		x = matchlen(old+I[st], oldsize-I[st], new, newsize);
		y = matchlen(old+I[en], oldsize-I[en], new, newsize);

		if (x > y) {
			*pos = I[st];
			return x;
		} else {
			*pos = I[en];
			return y;
		}
	};

	x = st+(en-st)/2;
	if (memcmp(old+I[x], new, MIN(oldsize-I[x],newsize)) < 0) {
		return search(I, old, oldsize, new, newsize, x, en, pos);
	} else {
		return search(I, old, oldsize, new, newsize, st, x, pos);
	};
}

static void offtout(int64_t x, uint8_t *buf)
{
	int64_t y;

	if (x < 0)
		y = -x; 
	else
		y = x;

				 buf[0] = y % 256; y -= buf[0];
	y = y / 256; buf[1] = y % 256; y -= buf[1];
	y = y / 256; buf[2] = y % 256; y -= buf[2];
	y = y / 256; buf[3] = y % 256; y -= buf[3];
	y = y / 256; buf[4] = y % 256; y -= buf[4];
	y = y / 256; buf[5] = y % 256; y -= buf[5];
	y = y / 256; buf[6] = y % 256; y -= buf[6];
	y = y / 256; buf[7] = y % 256;

	if (x < 0)
		buf[7] |= 0x80;
}

int bsdiff(
	struct bsdiff_ctx *ctx,
	struct bsdiff_stream *oldfile, 
	struct bsdiff_stream *newfile, 
	struct bsdiff_stream *patchfile)
{
	int ret;
	uint8_t *old = NULL, *new = NULL;
	int64_t oldsize, newsize, patchsize, patchsize2;
	int64_t *I = NULL, *V = NULL;
	int64_t scan, pos, len;
	int64_t lastscan, lastpos, lastoffset;
	int64_t oldscore, scsc;
	int64_t s, Sf, lenf, Sb, lenb;
	int64_t overlap, Ss, lens;
	int64_t i;
	int64_t dblen, eblen;
	uint8_t *db = NULL, *eb = NULL;
	uint8_t header[32], buf[24];
	struct bsdiff_compressor pfbz2 = { 0 };
	size_t cb;

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if ((oldfile->seek(oldfile->state, 0, SEEK_END) != BSDIFF_SUCCESS) ||
		(oldfile->tell(oldfile->state, &oldsize) != BSDIFF_SUCCESS) ||
		(oldfile->seek(oldfile->state, 0, SEEK_SET) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "get the size of oldfile");
	}
	if ((oldsize >= SIZE_MAX) || ((oldsize + 1) * sizeof(int64_t) >= SIZE_MAX))
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "the oldfile is too large");
	if ((old = malloc((size_t)(oldsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for old");
	if (oldfile->read(oldfile->state, old, oldsize, &cb) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read oldfile");

	cb = (size_t)((oldsize + 1) * sizeof(int64_t));
	if (((I = malloc(cb)) == NULL) || ((V = malloc(cb)) == NULL))
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for I && V");

	qsufsort(I, V, old, oldsize);

	free(V);
	V = NULL;

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if ((newfile->seek(newfile->state, 0, SEEK_END) != BSDIFF_SUCCESS) ||
		(newfile->tell(newfile->state, &newsize) != BSDIFF_SUCCESS) ||
		(newfile->seek(newfile->state, 0, SEEK_SET) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "get the size of newfile");
	}
	if (newsize >= SIZE_MAX)
		HANDLE_ERROR(BSDIFF_SIZE_TOO_LARGE, "the newfile is too large");
	if ((new = malloc((size_t)(newsize + 1))) == NULL)
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for new");
	if (newfile->read(newfile->state, new, newsize, &cb) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read newfile");

	if (((db = malloc((size_t)(newsize + 1))) == NULL) ||
		((eb = malloc((size_t)(newsize + 1))) == NULL))
	{
		HANDLE_ERROR(BSDIFF_OUT_OF_MEMORY, "malloc for db && eb");
	}
	dblen = 0;
	eblen = 0;

	/* Header is
		0	8	"BSDIFF40"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of new file */
	/* File is
		0	32	Header
		32	??	Bzip2ed ctrl block
		??	??	Bzip2ed diff block
		??	??	Bzip2ed extra block */
	memset(header, 0, 32);              /* Fix MSVC warning C6385 */
	memcpy(header, "BSDIFF40", 8);
	offtout(0, header + 8);
	offtout(0, header + 16);
	offtout(newsize, header + 24);
	if (patchfile->write(patchfile->state, header, 32) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "write header");

	/* Compute the differences, writing ctrl as we go */
	if ((bsdiff_create_bz2_compressor(&pfbz2) != BSDIFF_SUCCESS) ||
		(pfbz2.init(pfbz2.state, patchfile) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_ERROR, "create compressor for control block");
	}
	scan = 0; len = 0;
	lastscan = 0; lastpos = 0; lastoffset = 0;
	while (scan < newsize) {
		oldscore = 0;

		for (scsc = scan+=len; scan < newsize; scan++) {
			len = search(I, old, oldsize, new+scan, newsize-scan,
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

			for (i = 0; i < lenf; i++)
				db[dblen+i] = new[lastscan+i]-old[lastpos+i];
			for(i = 0; i < (scan-lenb)-(lastscan+lenf); i++)
				eb[eblen+i] = new[lastscan+lenf+i];

			dblen += lenf;
			eblen += (scan-lenb)-(lastscan+lenf);

			offtout(lenf, buf);
			offtout((scan-lenb)-(lastscan+lenf), buf + 8);
			offtout((pos-lenb)-(lastpos+lenf), buf + 16);
			if (pfbz2.write(pfbz2.state, buf, 24) != BSDIFF_SUCCESS)
				HANDLE_ERROR(BSDIFF_ERROR, "write ctrl data");

			lastscan = scan - lenb;
			lastpos = pos - lenb;
			lastoffset = pos - scan;
		};
	};
	if (pfbz2.flush(pfbz2.state) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "flush ctrl data");
	bsdiff_close_compressor(&pfbz2);

	/* Compute size of compressed ctrl data */
	if (patchfile->tell(patchfile->state, &patchsize) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read patchsize");
	offtout(patchsize - 32, header + 8);

	/* Write compressed diff data */
	if ((bsdiff_create_bz2_compressor(&pfbz2) != BSDIFF_SUCCESS) ||
		(pfbz2.init(pfbz2.state, patchfile) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_ERROR, "create compressor for diff block");
	}
	if (pfbz2.write(pfbz2.state, db, dblen) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "write diff data");
	if (pfbz2.flush(pfbz2.state) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "flush diff data");
	bsdiff_close_compressor(&pfbz2);

	/* Compute size of compressed diff data */
	if (patchfile->tell(patchfile->state, &patchsize2) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "read patchsize2");
	offtout(patchsize2 - patchsize, header + 16);

	/* Write compressed extra data */
	if ((bsdiff_create_bz2_compressor(&pfbz2) != BSDIFF_SUCCESS) ||
		(pfbz2.init(pfbz2.state, patchfile) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_ERROR, "create compressor for extra block");
	}
	if (pfbz2.write(pfbz2.state, db, dblen) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "write extra data");
	if (pfbz2.flush(pfbz2.state) != BSDIFF_SUCCESS)
		HANDLE_ERROR(BSDIFF_ERROR, "flush extra data");
	bsdiff_close_compressor(&pfbz2);

	/* Seek to the beginning, (re)write the header */
	if ((patchfile->seek(patchfile->state, 0, SEEK_SET) != BSDIFF_SUCCESS) ||
		(patchfile->write(patchfile->state, header, 32) != BSDIFF_SUCCESS) ||
		(patchfile->flush(patchfile->state) != BSDIFF_SUCCESS))
	{
		HANDLE_ERROR(BSDIFF_FILE_ERROR, "rewrite header");
	}

	ret = BSDIFF_SUCCESS;

cleanup:
	bsdiff_close_compressor(&pfbz2);
	if (db != NULL) { free(db); }
	if (eb != NULL) { free(eb); }
	if (I != NULL) { free(I); }
	if (old != NULL) { free(old); }
	if (new != NULL) { free(new); }

	return ret;
}
