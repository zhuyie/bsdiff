#include "bsdiff.h"
#include "bsdiff_private.h"
#include <stdlib.h>
#include <string.h>
#include <bzlib.h>

struct bz2_compressor
{
	int initialized;
	struct bsdiff_stream *strm;
	bz_stream bzstrm;
	int bzerr;
	char buf[5000];
};

static int bz2_compressor_init(void *state, struct bsdiff_stream *stream)
{
	struct bz2_compressor *enc = (struct bz2_compressor*)state;

	if (enc->initialized)
		return BSDIFF_ERROR;

	if (stream->read != NULL || stream->write == NULL || stream->flush == NULL)
		return BSDIFF_INVALID_ARG;
	enc->strm = stream;

	enc->bzstrm.bzalloc = NULL;
	enc->bzstrm.bzfree = NULL;
	enc->bzstrm.opaque = NULL;
	if (BZ2_bzCompressInit(&(enc->bzstrm), 9, 0, 30) != BZ_OK)
		return BSDIFF_ERROR;
	enc->bzstrm.avail_in = 0;
	enc->bzstrm.next_in = NULL;
	enc->bzstrm.avail_out = (unsigned int)(sizeof(enc->buf));
	enc->bzstrm.next_out = enc->buf;

	enc->bzerr = BZ_OK;

	enc->initialized = 1;

	return BSDIFF_SUCCESS;
}

static int bz2_compressor_write(void *state, const void *buffer, size_t size)
{
	struct bz2_compressor *enc = (struct bz2_compressor*)state;
	
	if (!enc->initialized)
		return BSDIFF_ERROR;
	if (enc->bzerr != BZ_OK && enc->bzerr != BZ_RUN_OK)
		return BSDIFF_ERROR;
	if (size >= UINT32_MAX)
		return BSDIFF_INVALID_ARG;
	if (size == 0)
		return BSDIFF_SUCCESS;

	enc->bzstrm.avail_in = (unsigned int)size;
	enc->bzstrm.next_in = (char*)buffer;

	while (1) {
		/* compress some amount of data */
		enc->bzerr = BZ2_bzCompress(&(enc->bzstrm), BZ_RUN);
		if (enc->bzerr != BZ_RUN_OK)
			return BSDIFF_ERROR;

		/* out buffer is full */
		if (enc->bzstrm.avail_out == 0) {
			if (enc->strm->write(enc->strm->state, enc->buf, sizeof(enc->buf)) != BSDIFF_SUCCESS)
				return BSDIFF_ERROR;
			enc->bzstrm.next_out = enc->buf;
			enc->bzstrm.avail_out = (unsigned int)(sizeof(enc->buf));
		}

		/* all input has been consumed */
		if (enc->bzstrm.avail_in == 0)
			return BSDIFF_SUCCESS;
	}

	/* never reached */
	return BSDIFF_ERROR;
}

static int bz2_compressor_flush(void *state)
{
	struct bz2_compressor *enc = (struct bz2_compressor*)state;
	size_t cb;

	if (!enc->initialized)
		return BSDIFF_ERROR;
	if (enc->bzerr != BZ_OK && enc->bzerr != BZ_RUN_OK)
		return BSDIFF_ERROR;

	while (1) {
		/* compress remaining input */
		enc->bzerr = BZ2_bzCompress(&(enc->bzstrm), BZ_FINISH);
		if (enc->bzerr != BZ_FINISH_OK && enc->bzerr != BZ_STREAM_END)
			return BSDIFF_ERROR;

		/* writing out the compressed output */
		if (enc->bzstrm.avail_out < (unsigned int)(sizeof(enc->buf))) {
			cb = sizeof(enc->buf) - enc->bzstrm.avail_out;
			if (enc->strm->write(enc->strm->state, enc->buf, cb) != BSDIFF_SUCCESS)
				return BSDIFF_ERROR;
			enc->bzstrm.avail_out = (unsigned int)(sizeof(enc->buf));
			enc->bzstrm.next_out = enc->buf;
		}

		/* done */
		if (enc->bzerr == BZ_STREAM_END) {
			/* flush the stream */
			if (enc->strm->flush(enc->strm->state) != BSDIFF_SUCCESS)
				return BSDIFF_ERROR;
			return BSDIFF_SUCCESS;
		}
	}

	/* never reached */
	return BSDIFF_ERROR;
}

static void bz2_compressor_close(void *state)
{
	struct bz2_compressor *enc = (struct bz2_compressor*)state;

	if (enc->initialized) {
		/* cleanup BZ2 compress state */
		BZ2_bzCompressEnd(&(enc->bzstrm));
	}

	/* free the state */
	free(enc);
}

int bsdiff_create_bz2_compressor(
	struct bsdiff_compressor *enc)
{
	struct bz2_compressor *state;

	state = malloc(sizeof(struct bz2_compressor));
	if (!state)
		return BSDIFF_OUT_OF_MEMORY;
	state->initialized = 0;
	state->strm = NULL;

	memset(enc, 0, sizeof(*enc));
	enc->state = state;
	enc->init = bz2_compressor_init;
	enc->write = bz2_compressor_write;
	enc->flush = bz2_compressor_flush;
	enc->close = bz2_compressor_close;

	return BSDIFF_SUCCESS;
}
