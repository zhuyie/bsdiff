#include "bsdiff.h"
#include "bsdiff_private.h"
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

struct zstd_enc_state
{
	struct bsdiff_stream *stream;
	ZSTD_CStream *cctx;
	ZSTD_outBuffer out;
	size_t out_capacity;
	void *out_buffer;
};

static int zstd_enc_init(void *state, struct bsdiff_stream *stream)
{
	struct zstd_enc_state *enc = (struct zstd_enc_state *)state;
	size_t ret;

	enc->stream = stream;

	enc->cctx = ZSTD_createCStream();
	if (!enc->cctx)
		return BSDIFF_ERROR;

	ret = ZSTD_initCStream(enc->cctx, ZSTD_CLEVEL_DEFAULT);
	if (ZSTD_isError(ret))
		return BSDIFF_ERROR;

	enc->out_capacity = ZSTD_CStreamOutSize();
	enc->out_buffer = malloc(enc->out_capacity);
	if (!enc->out_buffer)
		return BSDIFF_OUT_OF_MEMORY;

	enc->out.dst = enc->out_buffer;
	enc->out.size = enc->out_capacity;
	enc->out.pos = 0;

	return BSDIFF_SUCCESS;
}

static int zstd_enc_write(void *state, const void *buffer, size_t size)
{
	struct zstd_enc_state *enc = (struct zstd_enc_state *)state;
	ZSTD_inBuffer in = {buffer, size, 0};
	size_t ret;

	while (in.pos < in.size) {
		ret = ZSTD_compressStream(enc->cctx, &enc->out, &in);
		if (ZSTD_isError(ret))
			return BSDIFF_ERROR;

		if (enc->out.pos == enc->out.size) {
			if (enc->stream->write(enc->stream->state, enc->out_buffer,
			                       enc->out.pos) != BSDIFF_SUCCESS)
				return BSDIFF_FILE_ERROR;
			enc->out.pos = 0;
		}
	}

	return BSDIFF_SUCCESS;
}

static int zstd_enc_flush(void *state)
{
	struct zstd_enc_state *enc = (struct zstd_enc_state *)state;
	size_t rem = ZSTD_endStream(enc->cctx, &enc->out);

	while (rem > 0 || enc->out.pos > 0) {
		if (enc->out.pos > 0) {
			if (enc->stream->write(enc->stream->state, enc->out_buffer,
			                       enc->out.pos) != BSDIFF_SUCCESS)
				return BSDIFF_FILE_ERROR;
			enc->out.pos = 0;
		}
		if (rem > 0)
			rem = ZSTD_endStream(enc->cctx, &enc->out);
	}

	return BSDIFF_SUCCESS;
}

static void zstd_enc_close(void *state)
{
	struct zstd_enc_state *enc = (struct zstd_enc_state *)state;

	if (enc->cctx)
		ZSTD_freeCStream(enc->cctx);
	if (enc->out_buffer)
		free(enc->out_buffer);
	free(enc);
}

int bsdiff_create_zstd_compressor(struct bsdiff_compressor *enc)
{
	struct zstd_enc_state *state;

	state = malloc(sizeof(struct zstd_enc_state));
	if (!state)
		return BSDIFF_OUT_OF_MEMORY;
	
	memset(state, 0, sizeof(struct zstd_enc_state));

	enc->state = state;
	enc->init = zstd_enc_init;
	enc->write = zstd_enc_write;
	enc->flush = zstd_enc_flush;
	enc->close = zstd_enc_close;

	return BSDIFF_SUCCESS;
}
