# bsdiff

[bsdiff](https://github.com/zhuyie/bsdiff) is a library for building and applying patches to binary files.

The original algorithm and implementation was developed by Colin Percival. The algorithm is described in his (unpublished) [paper](https://www.daemonology.net/papers/bsdiff.pdf). For more information visit his website at <http://www.daemonology.net/bsdiff/>.

I maintain this project separately from Colin's work, with the following goals:
* Ability to easily embed the routines as a library instead of an external binary.
* Compatible with the original patch format and can easily adapt to new patch format.
* Support memory-based input/output stream.
* Self-contained 3rd-party libraries, build on Windows/Linux/OSX.

## API
```c
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
```

## Usage
```c
#include <stdio.h>
#include "bsdiff.h"

static void log_error(void *opaque, const char *errmsg)
{
	(void)opaque;
	fprintf(stderr, "%s", errmsg);
}

int generate_patch(const char *oldname, const char *newname, const char *patchname)
{
	int ret = 1;
	struct bsdiff_stream oldfile = { 0 }, newfile = { 0 }, patchfile = { 0 };
	struct bsdiff_ctx ctx = { 0 };
	struct bsdiff_patch_packer packer = { 0 };

	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, oldname, &oldfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open oldfile: %s\n", oldname);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, newname, &newfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open newfile: %s\n", newname);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_WRITE, patchname, &patchfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open patchfile: %s\n", patchname);
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

int apply_patch(const char *oldname, const char *newname, const char *patchname)
{
	int ret = 1;
	struct bsdiff_stream oldfile = { 0 }, newfile = { 0 }, patchfile = { 0 };
	struct bsdiff_ctx ctx = { 0 };
	struct bsdiff_patch_packer packer = { 0 };

	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, oldname, &oldfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open oldfile: %s\n", oldname);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_WRITE, newname, &newfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open newfile: %s\n", newname);
		goto cleanup;
	}
	if ((ret = bsdiff_open_file_stream(BSDIFF_MODE_READ, patchname, &patchfile)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't open patchfile: %s\n", patchname);
		goto cleanup;
	}
	if ((ret = bsdiff_open_bz2_patch_packer(BSDIFF_MODE_READ, &patchfile, &packer)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "can't create BZ2 patch packer\n");
		goto cleanup;
	}

	ctx.log_error = log_error;
	
	if ((ret = bspatch(&ctx, &oldfile, &newfile, &packer)) != BSDIFF_SUCCESS) {
		fprintf(stderr, "bspatch failed: %d\n", ret);
		goto cleanup;
	}

cleanup:
	bsdiff_close_patch_packer(&packer);
	bsdiff_close_stream(&patchfile);
	bsdiff_close_stream(&newfile);
	bsdiff_close_stream(&oldfile);

	return ret;
}
```
