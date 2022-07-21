#include "bsdiff.h"
#include "bsdiff_private.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct substream_state
{
	struct bsdiff_stream *base;
	int64_t start;
	int64_t end;
	int64_t current;
};

static int substream_seek(void *state, int64_t offset, int origin)
{
	struct substream_state *substream = (struct substream_state*)state;
	/* only support SEEK_SET */
	if (origin != BSDIFF_SEEK_SET)
		return BSDIFF_INVALID_ARG;
	if (offset < substream->start || offset > substream->end)
		return BSDIFF_INVALID_ARG;
	substream->current = offset;
	return BSDIFF_SUCCESS;
}

static int substream_tell(void *state, int64_t *position)
{
	struct substream_state *substream = (struct substream_state*)state;
	*position = substream->current;
	return BSDIFF_SUCCESS;
}

static int substream_read(void *state, void *buffer, size_t size, size_t *readed)
{
	int ret;
	struct substream_state *substream = (struct substream_state*)state;
	struct bsdiff_stream *base = substream->base;
	size_t cb;

	*readed = 0;
	
	if (size == 0)
		return BSDIFF_SUCCESS;
	if (substream->current == substream->end)
		return BSDIFF_END_OF_FILE;

	/* calculate the number of bytes to read */
	cb = size;
	if (substream->current + (int64_t)size > substream->end)
		cb = (size_t)(substream->end - substream->current);
	/* (re)seek to current */
	if (base->seek(base->state, substream->current, BSDIFF_SEEK_SET) != BSDIFF_SUCCESS)
		return BSDIFF_FILE_ERROR;
	/* read */
	ret = base->read(base->state, buffer, cb, readed);
	/* update current */
	if (ret == BSDIFF_SUCCESS || ret == BSDIFF_END_OF_FILE)
		substream->current += *readed;

	return ret;
}

static void substream_close(void *state)
{
	struct substream_state *substream = (struct substream_state*)state;
	/* free the state */
	free(substream);
}

static int substream_getmode(void *state)
{
	return BSDIFF_MODE_READ;
}

int bsdiff_open_substream(
	struct bsdiff_stream *base,
	int64_t read_start,
	int64_t read_end,
	struct bsdiff_stream *substream)
{
	int64_t pos, base_size;
	struct substream_state *state;

	/* base stream should be read-only */
	if (base->get_mode(base->state) != BSDIFF_MODE_READ)
		return BSDIFF_INVALID_ARG;

	/* check region */
	if ((base->tell(base->state, &pos) != BSDIFF_SUCCESS) ||
		(base->seek(base->state, 0, BSDIFF_SEEK_END) != BSDIFF_SUCCESS) ||
		(base->tell(base->state, &base_size) != BSDIFF_SUCCESS) ||
		(base->seek(base->state, pos, BSDIFF_SEEK_SET) != BSDIFF_SUCCESS))
	{
		return BSDIFF_ERROR;
	}
	if (read_start < 0 || read_end <= read_start || read_end > base_size)
		return BSDIFF_INVALID_ARG;

	state = malloc(sizeof(struct substream_state));
	if (state == NULL)
		return BSDIFF_OUT_OF_MEMORY;
	state->base = base;
	state->start = read_start;
	state->end = read_end;
	state->current = state->start;

	memset(substream, 0, sizeof(*substream));
	substream->state = state;
	substream->close = substream_close;
	substream->get_mode = substream_getmode;
	substream->seek = substream_seek;
	substream->tell = substream_tell;
	substream->read = substream_read;

	return BSDIFF_SUCCESS;
}
