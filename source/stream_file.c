#include "bsdiff.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int filestream_seek(void *state, int64_t offset, int origin)
{
	int n;
	FILE *f = (FILE*)state;
#if defined(_MSC_VER)
	n = _fseeki64(f, offset, origin);
	return (n != 0) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#else
	n = fseek(f, offset, origin);
	return (n != 0) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#endif
}

static int filestream_tell(void *state, int64_t *position)
{
	FILE *f = (FILE*)state;
#if defined(_MSC_VER)
	*position = _ftelli64(f);
	return (*position == -1) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#else
	*position = ftell(f);
	return (*position == -1) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#endif
}

static int filestream_read(void *state, void *buffer, size_t size, size_t *readed)
{
	FILE *f = (FILE*)state;

	*readed = 0;

	/* The ANSI standard requires a return value of 0 for a size of 0. */
	if (size == 0)
		return BSDIFF_SUCCESS;

	*readed = fread(buffer, 1, size, f);
	if (*readed < size)
		return feof(f) ? BSDIFF_END_OF_FILE : BSDIFF_FILE_ERROR;

	return BSDIFF_SUCCESS;
}

static int filestream_write(void *state, const void *buffer, size_t size)
{
	FILE *f = (FILE*)state;
	size_t n = fwrite(buffer, 1, size, f);
	return (n < size) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
}

static int filestream_flush(void *state)
{
	FILE *f = (FILE*)state;
	return fflush(f) != 0 ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
}

static void filestream_close(void *state)
{
	FILE *f = (FILE*)state;
	fclose(f);
}

static int filestream_getmode_read(void *state)
{
	return BSDIFF_MODE_READ;
}

static int filestream_getmode_write(void *state)
{
	return BSDIFF_MODE_WRITE;
}

int bsdiff_open_file_stream(
	int mode,
	const char *filename, 
	struct bsdiff_stream *stream)
{
	FILE *f;
	assert(mode >= BSDIFF_MODE_READ && mode <= BSDIFF_MODE_WRITE);
	assert(filename);
	assert(stream);

	f = fopen(filename, (mode == BSDIFF_MODE_WRITE) ? "wb" : "rb");
	if (f == NULL)
		return BSDIFF_FILE_ERROR;

	memset(stream, 0, sizeof(*stream));
	stream->state = f;
	stream->close = filestream_close;
	stream->seek = filestream_seek;
	stream->tell = filestream_tell;
	if (mode != BSDIFF_MODE_WRITE) {
		stream->get_mode = filestream_getmode_read;
		stream->read = filestream_read;
	} else {
		stream->get_mode = filestream_getmode_write;
		stream->write = filestream_write;
		stream->flush = filestream_flush;
	}

	return BSDIFF_SUCCESS;
}
