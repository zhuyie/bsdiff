#include "bsdiff.h"
#include "bsdiff_private.h"
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

struct zstd_dec_state
{
	struct bsdiff_stream *stream;
	ZSTD_DStream *dctx;
	ZSTD_inBuffer in;
	void *in_buffer;
	size_t in_capacity;
};

static int zstd_dec_init(void *state, struct bsdiff_stream *stream)
{
	struct zstd_dec_state *dec = (struct zstd_dec_state *)state;
	size_t ret;

	dec->stream = stream;

	dec->dctx = ZSTD_createDStream();
	if (!dec->dctx)
		return BSDIFF_ERROR;

	ret = ZSTD_initDStream(dec->dctx);
	if (ZSTD_isError(ret))
		return BSDIFF_ERROR;

	dec->in_capacity = ZSTD_DStreamInSize();
	dec->in_buffer = malloc(dec->in_capacity);
	if (!dec->in_buffer)
		return BSDIFF_OUT_OF_MEMORY;

	dec->in.src = dec->in_buffer;
	dec->in.size = 0;
	dec->in.pos = 0;

	return BSDIFF_SUCCESS;
}

static int zstd_dec_read(void *state, void *buffer, size_t size, size_t *readed)
{
	struct zstd_dec_state *dec = (struct zstd_dec_state *)state;
	ZSTD_outBuffer out = {buffer, size, 0};
	size_t bread, ret;
	int r;

	*readed = 0;

	while (out.pos < out.size) {
		if (dec->in.pos == dec->in.size) {
			bread = 0;
			r = dec->stream->read(dec->stream->state, dec->in_buffer,
			                      dec->in_capacity, &bread);
			if (r != BSDIFF_SUCCESS && r != BSDIFF_END_OF_FILE)
				return BSDIFF_FILE_ERROR;

			if (bread == 0 && r == BSDIFF_END_OF_FILE) {
				*readed = out.pos;
				if (out.pos == 0)
					return BSDIFF_END_OF_FILE;
				return BSDIFF_SUCCESS;
			}
			dec->in.size = bread;
			dec->in.pos = 0;
		}

		ret = ZSTD_decompressStream(dec->dctx, &out, &dec->in);
		if (ZSTD_isError(ret))
			return BSDIFF_ERROR;
	}

	*readed = out.pos;
	return BSDIFF_SUCCESS;
}

static void zstd_dec_close(void *state)
{
	struct zstd_dec_state *dec = (struct zstd_dec_state *)state;

	if (dec->dctx)
		ZSTD_freeDStream(dec->dctx);
	if (dec->in_buffer)
		free(dec->in_buffer);
	free(dec);
}

int bsdiff_create_zstd_decompressor(struct bsdiff_decompressor *dec)
{
	struct zstd_dec_state *state;

	state = malloc(sizeof(struct zstd_dec_state));
	if (!state)
		return BSDIFF_OUT_OF_MEMORY;
	
	memset(state, 0, sizeof(struct zstd_dec_state));

	dec->state = state;
	dec->init = zstd_dec_init;
	dec->read = zstd_dec_read;
	dec->close = zstd_dec_close;

	return BSDIFF_SUCCESS;
}
