#include "bsdiff.h"
#include "bsdiff_mem.h"
#include "bsdiff_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bsdiff_create_zstd_compressor(struct bsdiff_compressor *enc);
int bsdiff_create_zstd_decompressor(struct bsdiff_decompressor *dec);

static int64_t zstd_read_int64(uint8_t *buf)
{
	uint64_t y = ((uint64_t)buf[0]) |
	             ((uint64_t)buf[1] << 8) |
	             ((uint64_t)buf[2] << 16) |
	             ((uint64_t)buf[3] << 24) |
	             ((uint64_t)buf[4] << 32) |
	             ((uint64_t)buf[5] << 40) |
	             ((uint64_t)buf[6] << 48) |
	             ((uint64_t)buf[7] << 56);
	return (int64_t)((y >> 1) ^ (0ULL - (y & 1)));
}

static void zstd_write_int64(int64_t x, uint8_t *buf)
{
	uint64_t y = ((uint64_t)x << 1) ^ (uint64_t)((x < 0) ? ~0ULL : 0ULL);

	buf[0] = (uint8_t)(y);
	buf[1] = (uint8_t)(y >> 8);
	buf[2] = (uint8_t)(y >> 16);
	buf[3] = (uint8_t)(y >> 24);
	buf[4] = (uint8_t)(y >> 32);
	buf[5] = (uint8_t)(y >> 40);
	buf[6] = (uint8_t)(y >> 48);
	buf[7] = (uint8_t)(y >> 56);
}

struct zstd_patch_packer
{
	struct bsdiff_stream *stream;
	int mode;

	int64_t new_size;

	int64_t header_x;
	int64_t header_y;
	int64_t header_z;

	struct bsdiff_stream cpf;
	struct bsdiff_stream dpf;
	struct bsdiff_stream epf;
	struct bsdiff_decompressor cpf_dec;
	struct bsdiff_decompressor dpf_dec;
	struct bsdiff_decompressor epf_dec;

	struct bsdiff_compressor enc;
	uint8_t *db;
	uint8_t *eb;
	int64_t dblen;
	int64_t eblen;
};

static int zstd_patch_packer_read_new_size(void *state, int64_t *size)
{
	int ret;
	uint8_t header[32];
	size_t cb;
	int64_t ctrllen, datalen, newsize;
	int64_t read_start, read_end;

	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_READ);
	assert(packer->new_size == -1);

	/* Read header */
	ret = packer->stream->read(packer->stream->state, header, 32, &cb);
	if (ret != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;

	/* Check for appropriate magic */
	if (memcmp(header, "ZSTDDIFF", 8) != 0)
		return BSDIFF_CORRUPT_PATCH;

	/* Read lengths from header */
	ctrllen = zstd_read_int64(header + 8);
	datalen = zstd_read_int64(header + 16);
	newsize = zstd_read_int64(header + 24);
	if ((ctrllen < 0) || (datalen < 0) || (newsize < 0))
		return BSDIFF_CORRUPT_PATCH;

	/* Open substreams and create decompressors */
	/* control block */
	read_start = 32;
	read_end = read_start + ctrllen;
	if (bsdiff_open_substream(packer->stream, read_start, read_end,
	                          &(packer->cpf)) != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	if (bsdiff_create_zstd_decompressor(&(packer->cpf_dec)) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	if (packer->cpf_dec.init(packer->cpf_dec.state, &(packer->cpf)) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	/* diff block */
	read_start = read_end;
	read_end = read_start + datalen;
	if (bsdiff_open_substream(packer->stream, read_start, read_end,
	                          &(packer->dpf)) != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	if (bsdiff_create_zstd_decompressor(&(packer->dpf_dec)) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	if (packer->dpf_dec.init(packer->dpf_dec.state, &(packer->dpf)) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	/* extra block */
	read_start = read_end;
	if ((packer->stream->seek(packer->stream->state, 0, BSDIFF_SEEK_END) !=
	     BSDIFF_SUCCESS) ||
	    (packer->stream->tell(packer->stream->state, &read_end) !=
	     BSDIFF_SUCCESS)) {
		return BSDIFF_FILE_ERROR;
	}
	if (bsdiff_open_substream(packer->stream, read_start, read_end,
	                          &(packer->epf)) != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	if (bsdiff_create_zstd_decompressor(&(packer->epf_dec)) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	if (packer->epf_dec.init(packer->epf_dec.state, &(packer->epf)) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_ERROR;

	packer->new_size = newsize;

	*size = packer->new_size;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_read_entry_header(void *state, int64_t *diff,
                                               int64_t *extra, int64_t *seek)
{
	int ret;
	uint8_t buf[24];
	size_t cb;

	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_READ);
	assert(packer->new_size >= 0);
	assert(packer->header_x == 0 && packer->header_y == 0);

	ret = packer->cpf_dec.read(packer->cpf_dec.state, buf, 24, &cb);
	if ((ret != BSDIFF_SUCCESS && ret != BSDIFF_END_OF_FILE) || (cb != 24))
		return BSDIFF_ERROR;
	packer->header_x = zstd_read_int64(buf);
	packer->header_y = zstd_read_int64(buf + 8);
	packer->header_z = zstd_read_int64(buf + 16);

	*diff = packer->header_x;
	*extra = packer->header_y;
	*seek = packer->header_z;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_read_entry_diff(void *state, void *buffer,
                                             size_t size, size_t *readed)
{
	int ret;
	int64_t cb;

	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_READ);
	assert(packer->new_size >= 0);
	assert(packer->header_x >= 0);

	*readed = 0;

	cb = (int64_t)size;
	if (packer->header_x < cb)
		cb = packer->header_x;
	if (cb <= 0)
		return BSDIFF_END_OF_FILE;

	ret = packer->dpf_dec.read(packer->dpf_dec.state, buffer, (size_t)cb, readed);
	packer->header_x -= (int64_t)(*readed);
	return ret;
}

static int zstd_patch_packer_read_entry_extra(void *state, void *buffer,
                                              size_t size, size_t *readed)
{
	int ret;
	int64_t cb;

	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_READ);
	assert(packer->new_size >= 0);
	assert(packer->header_y >= 0);

	*readed = 0;

	cb = (int64_t)size;
	if (packer->header_y < cb)
		cb = packer->header_y;
	if (cb <= 0)
		return BSDIFF_END_OF_FILE;

	ret = packer->epf_dec.read(packer->epf_dec.state, buffer, (size_t)cb, readed);
	packer->header_y -= (int64_t)(*readed);
	return ret;
}

static int zstd_patch_packer_write_new_size(void *state, int64_t size)
{
	uint8_t header[32];
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_WRITE);
	assert(packer->new_size == -1);
	assert(size >= 0);

	memset(header, 0, sizeof(header));

	/* Write a pseudo header */
	if (packer->stream->write(packer->stream->state, header, 32) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;

	/* Initialize compressor for control block */
	if ((bsdiff_create_zstd_compressor(&(packer->enc)) != BSDIFF_SUCCESS) ||
	    (packer->enc.init(packer->enc.state, packer->stream) != BSDIFF_SUCCESS)) {
		return BSDIFF_ERROR;
	}

	/* Allocate memory for db && eb */
	assert(packer->db == NULL && packer->dblen == 0);
	assert(packer->eb == NULL && packer->eblen == 0);
	packer->db = bsdiff_malloc((size_t)(size + 1));
	packer->eb = bsdiff_malloc((size_t)(size + 1));
	if (!packer->db || !packer->eb)
		return BSDIFF_OUT_OF_MEMORY;
	packer->dblen = 0;
	packer->eblen = 0;

	packer->new_size = size;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_write_entry_header(void *state, int64_t diff,
                                                int64_t extra, int64_t seek)
{
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	uint8_t buf[24];
	int ret;

	assert(packer->mode == BSDIFF_MODE_WRITE);
	assert(packer->new_size >= 0);
	assert(diff >= 0);
	assert(extra >= 0);

	assert(packer->header_x == 0 && packer->header_y == 0);
	packer->header_x = diff;
	packer->header_y = extra;
	packer->header_z = seek;

	/* Write a triple */
	zstd_write_int64(packer->header_x, buf);
	zstd_write_int64(packer->header_y, buf + 8);
	zstd_write_int64(packer->header_z, buf + 16);
	ret = packer->enc.write(packer->enc.state, buf, 24);
	if (ret != BSDIFF_SUCCESS)
		return ret;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_write_entry_diff(void *state, const void *buffer,
                                              size_t size)
{
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_WRITE);
	assert(packer->new_size >= 0);

	if ((int64_t)size > packer->header_x)
		return BSDIFF_INVALID_ARG;
	if (packer->dblen + (int64_t)size > packer->new_size)
		return BSDIFF_INVALID_ARG;
	memcpy(packer->db + packer->dblen, buffer, size);
	packer->dblen += (int64_t)size;
	packer->header_x -= (int64_t)size;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_write_entry_extra(void *state, const void *buffer,
                                               size_t size)
{
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	assert(packer->mode == BSDIFF_MODE_WRITE);
	assert(packer->new_size >= 0);

	if ((int64_t)size > packer->header_y)
		return BSDIFF_INVALID_ARG;
	if (packer->eblen + (int64_t)size > packer->new_size)
		return BSDIFF_INVALID_ARG;
	memcpy(packer->eb + packer->eblen, buffer, size);
	packer->eblen += (int64_t)size;
	packer->header_y -= (int64_t)size;

	return BSDIFF_SUCCESS;
}

static int zstd_patch_packer_flush(void *state)
{
	uint8_t header[32];
	int64_t patchsize, patchsize2;
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;

	assert(packer->mode == BSDIFF_MODE_WRITE);
	assert(packer->new_size >= 0);
	assert(packer->header_x == 0 && packer->header_y == 0);

	memset(header, 0, sizeof(header));
	memcpy(header, "ZSTDDIFF", 8);
	zstd_write_int64(packer->new_size, header + 24);

	/* Flush ctrl data */
	if (packer->enc.flush(packer->enc.state) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	bsdiff_close_compressor(&(packer->enc));

	/* Compute size of compressed ctrl data */
	if (packer->stream->tell(packer->stream->state, &patchsize) != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	zstd_write_int64(patchsize - 32, header + 8);

	/* Write compressed diff data */
	if ((bsdiff_create_zstd_compressor(&(packer->enc)) != BSDIFF_SUCCESS) ||
	    (packer->enc.init(packer->enc.state, packer->stream) != BSDIFF_SUCCESS)) {
		return BSDIFF_ERROR;
	}
	if (packer->enc.write(packer->enc.state, packer->db, (size_t)packer->dblen) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	if (packer->enc.flush(packer->enc.state) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	bsdiff_close_compressor(&(packer->enc));

	/* Compute size of compressed diff data */
	if (packer->stream->tell(packer->stream->state, &patchsize2) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	zstd_write_int64(patchsize2 - patchsize, header + 16);

	/* Write compressed extra data */
	if ((bsdiff_create_zstd_compressor(&(packer->enc)) != BSDIFF_SUCCESS) ||
	    (packer->enc.init(packer->enc.state, packer->stream) != BSDIFF_SUCCESS)) {
		return BSDIFF_ERROR;
	}
	if (packer->enc.write(packer->enc.state, packer->eb, (size_t)packer->eblen) !=
	    BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	if (packer->enc.flush(packer->enc.state) != BSDIFF_SUCCESS)
		return BSDIFF_ERROR;
	bsdiff_close_compressor(&(packer->enc));

	/* Seek to the beginning, (re)write the header */
	if ((packer->stream->seek(packer->stream->state, 0, BSDIFF_SEEK_SET) !=
	     BSDIFF_SUCCESS) ||
	    (packer->stream->write(packer->stream->state, header, 32) !=
	     BSDIFF_SUCCESS) ||
	    (packer->stream->flush(packer->stream->state) != BSDIFF_SUCCESS)) {
		return BSDIFF_FILE_ERROR;
	}

	return BSDIFF_SUCCESS;
}

static void zstd_patch_packer_close(void *state)
{
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;

	if (packer->mode == BSDIFF_MODE_READ) {
		bsdiff_close_decompressor(&(packer->cpf_dec));
		bsdiff_close_decompressor(&(packer->dpf_dec));
		bsdiff_close_decompressor(&(packer->epf_dec));
		bsdiff_close_stream(&(packer->cpf));
		bsdiff_close_stream(&(packer->dpf));
		bsdiff_close_stream(&(packer->epf));
	} else {
		bsdiff_close_compressor(&(packer->enc));
		bsdiff_free(packer->db);
		bsdiff_free(packer->eb);
	}

	bsdiff_free(packer);
}

static int zstd_patch_packer_getmode(void *state)
{
	struct zstd_patch_packer *packer = (struct zstd_patch_packer *)state;
	return packer->mode;
}

int bsdiff_open_zstd_patch_packer(int mode, struct bsdiff_stream *stream,
                                  struct bsdiff_patch_packer *packer)
{
	struct zstd_patch_packer *state;

	assert(mode >= BSDIFF_MODE_READ && mode <= BSDIFF_MODE_WRITE);
	assert(stream);
	assert(packer);

	state = bsdiff_malloc(sizeof(struct zstd_patch_packer));
	if (!state)
		return BSDIFF_OUT_OF_MEMORY;
	memset(state, 0, sizeof(*state));
	state->stream = stream;
	state->mode = mode;
	state->new_size = -1;

	memset(packer, 0, sizeof(*packer));
	packer->state = state;
	if (mode == BSDIFF_MODE_READ) {
		packer->read_new_size = zstd_patch_packer_read_new_size;
		packer->read_entry_header = zstd_patch_packer_read_entry_header;
		packer->read_entry_diff = zstd_patch_packer_read_entry_diff;
		packer->read_entry_extra = zstd_patch_packer_read_entry_extra;
	} else {
		packer->write_new_size = zstd_patch_packer_write_new_size;
		packer->write_entry_header = zstd_patch_packer_write_entry_header;
		packer->write_entry_diff = zstd_patch_packer_write_entry_diff;
		packer->write_entry_extra = zstd_patch_packer_write_entry_extra;
		packer->flush = zstd_patch_packer_flush;
	}
	packer->close = zstd_patch_packer_close;
	packer->get_mode = zstd_patch_packer_getmode;

	return BSDIFF_SUCCESS;
}
