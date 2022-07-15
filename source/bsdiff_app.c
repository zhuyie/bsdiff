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

#include <stdio.h>
#include "bsdiff.h"

static void log_error(void *opaque, const char *errmsg)
{
	(void)opaque;
	fprintf(stderr, "%s", errmsg);
}

int main(int argc, char * argv[])
{
	int ret = 1;
	struct bsdiff_stream oldfile = { 0 }, newfile = { 0 }, patchfile = { 0 };
	struct bsdiff_ctx ctx = { 0 };
	struct bsdiff_patch_packer packer = { 0 };

	if (argc != 4) {
		fprintf(stderr, "usage: %s oldfile newfile patchfile\n", argv[0]);
		return 1;
	}

	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, argv[1], &oldfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open oldfile: %s\n", argv[1]);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, argv[2], &newfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open newfile: %s\n", argv[2]);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_WRITE, argv[3], &patchfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open patchfile: %s\n", argv[3]);
		goto cleanup;
	}
	if ((ret = bsdiff_open_bz2_patch_packer(BSDIFF_MODE_WRITE, &patchfile, &packer)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't create BZ2 patch packer\n");
		goto cleanup;
	}

	ctx.log_error = log_error;

	if ((ret = bsdiff(&ctx, &oldfile, &newfile, &packer)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "bsdiff failed: %d\n", ret);
		goto cleanup;
	}

cleanup:
	bsdiff_close_patch_packer(&packer);
	bsdiff_close_stream(&patchfile);
	bsdiff_close_stream(&newfile);
	bsdiff_close_stream(&oldfile);

	return ret;
}
