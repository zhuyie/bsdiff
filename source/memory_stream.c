#include "bsdiff.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct memstream_state
{
	int mode;
	void *buffer;
	size_t size;
	size_t capacity;
	size_t pos;
};

static int memstream_seek(void *state, int64_t offset, int origin)
{
	struct memstream_state *s = (struct memstream_state*)state;
	int64_t newpos = -1;

	switch (origin) {
	case BSDIFF_SEEK_SET:
		newpos = offset;
		break;
	case BSDIFF_SEEK_CUR:
		newpos = (int64_t)s->pos + offset;
		break;
	case BSDIFF_SEEK_END:
		newpos = (int64_t)s->size + offset;
		break;
	}
	if (newpos < 0 || newpos > (int64_t)s->size)
		return BSDIFF_INVALID_ARG;

	s->pos = (size_t)newpos;

	return BSDIFF_SUCCESS;
}

static int memstream_tell(void *state, int64_t *position)
{
	struct memstream_state *s = (struct memstream_state*)state;
	*position = (int64_t)s->pos;
	return BSDIFF_SUCCESS;
}

static int memstream_read(void *state, void *buffer, size_t size, size_t *readed)
{
	int ret;
	struct memstream_state *s = (struct memstream_state*)state;
	size_t cb;

	assert(s->mode == BSDIFF_MODE_READ);

	*readed = 0;
	
	if (size == 0)
		return BSDIFF_SUCCESS;

	cb = size;
	if (s->pos + size > s->size)
		cb = s->size - s->pos;

	memcpy(buffer, s->buffer + s->pos, cb);

	s->pos += cb;

	return (cb < size) ? BSDIFF_END_OF_FILE : BSDIFF_SUCCESS;
}

static size_t calc_new_capacity(size_t current, size_t required)
{
	size_t cap = current;
	while (cap < required) {
		/*
		  https://github.com/facebook/folly/blob/main/folly/docs/FBVector.md#memory-handling
		  Our strategy: empty() ? 64 : capacity() * 1.5
		 */
		if (cap == 0)
			cap = 64;
		else
			cap = (cap * 3 + 1) / 2;
	}
	return cap;
}

static int memstream_write(void *state, const void *buffer, size_t size)
{
	struct memstream_state *s = (struct memstream_state*)state;
	void *newbuf;
	size_t newcap;

	assert(s->mode == BSDIFF_MODE_WRITE);

	if (size == 0)
		return BSDIFF_SUCCESS;

	/* Grow the capacity if needed */
	if (s->pos + size > s->capacity) {
		newcap = calc_new_capacity(s->capacity, s->pos + size);

		newbuf = realloc(s->buffer, newcap);
		if (!newbuf)
			return BSDIFF_OUT_OF_MEMORY;

		s->buffer = newbuf;
		s->capacity = newcap;
	}

	/* memcpy */
	memcpy(s->buffer + s->pos, buffer, size);

	/* Update pos */
	s->pos += size;

	/* Update size */
	if (s->pos > s->size)
		s->size = s->pos;

	return BSDIFF_SUCCESS;
}

static int memstream_flush(void *state)
{
	struct memstream_state *s = (struct memstream_state*)state;

	assert(s->mode == BSDIFF_MODE_WRITE);
	(void)s;

	return BSDIFF_SUCCESS;
}

static int memstream_getbuffer(void *state, const void **ppbuffer, size_t *psize)
{
	struct memstream_state *s = (struct memstream_state*)state;

	*ppbuffer = s->buffer;
	*psize = s->size;

	return BSDIFF_SUCCESS;
}

static void memstream_close(void *state)
{
	struct memstream_state *s = (struct memstream_state*)state;

	if (s->mode == BSDIFF_MODE_WRITE) {
		free(s->buffer);
	}

	free(s);
}

static int memstream_getmode(void *state)
{
	struct memstream_state *s = (struct memstream_state*)state;
	return s->mode;
}

int bsdiff_open_memory_stream(
	const void *buffer, 
	size_t size,
	struct bsdiff_stream *stream)
{
	struct memstream_state *state;

	state = malloc(sizeof(struct memstream_state));
	if (state == NULL)
		return BSDIFF_OUT_OF_MEMORY;

	if (buffer != NULL && size > 0) {
		/* read mode */
		state->mode = BSDIFF_MODE_READ;
		state->buffer = (void*)buffer;
		state->capacity = size;
	} else {
		/* write mode */
		if (buffer != NULL || size > 0) {
			free(state);
			return BSDIFF_INVALID_ARG;
		}
		state->mode = BSDIFF_MODE_WRITE;
		state->buffer = NULL;
		state->capacity = 0;
	}
	state->size = 0;
	state->pos = 0;

	memset(stream, 0, sizeof(*stream));
	stream->state = state;
	stream->close = memstream_close;
	stream->get_mode = memstream_getmode;
	stream->seek = memstream_seek;
	stream->tell = memstream_tell;
	if (state->mode == BSDIFF_MODE_READ) {
		stream->read = memstream_read;
	} else {
		stream->write = memstream_write;
		stream->flush = memstream_flush;
	}
	stream->get_buffer = memstream_getbuffer;

	return BSDIFF_SUCCESS;
}
