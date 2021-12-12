#include "bsdiff.h"
#include <stdio.h>
#include <string.h>

static int bsdiff_stream_file_seek(void *state, int64_t offset, int origin)
{
	int n;
	FILE *f = (FILE*)state;
#if defined(_MSC_VER)
	// todo
#else
	n = fseek(f, offset, origin);
	return (n != 0) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#endif
}

static int bsdiff_stream_file_tell(void *state, int64_t *position)
{
	FILE *f = (FILE*)state;
#if defined(_MSC_VER)
	// todo
#else
	*position = ftell(f);
	return (*position == -1) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
#endif
}

static int bsdiff_stream_file_read(void *state, void *buffer, size_t size, size_t *readed)
{
	FILE *f = (FILE*)state;
	*readed = fread(buffer, 1, size, f);
	if (*readed < size)
		return feof(f) ? BSDIFF_END_OF_FILE : BSDIFF_FILE_ERROR;
	return BSDIFF_SUCCESS;
}

static int bsdiff_stream_file_write(void *state, const void *buffer, size_t size, size_t *written)
{
	FILE *f = (FILE*)state;
	*written = fwrite(buffer, 1, size, f);
	return (*written < size) ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
}

static int bsdiff_stream_file_flush(void *state)
{
	FILE *f = (FILE*)state;
	return fflush(f) != 0 ? BSDIFF_FILE_ERROR : BSDIFF_SUCCESS;
}

static void bsdiff_stream_file_close(void *state)
{
	FILE *f = (FILE*)state;
	fclose(f);
}

int bsdiff_open_file_stream(
	const char *filename, 
	int write,
	struct bsdiff_stream *stream)
{
	FILE *f;

	f = fopen(filename, write ? "wb" : "rb");
	if (f == NULL)
		return BSDIFF_FILE_ERROR;

	memset(stream, 0, sizeof(*stream));
	stream->state = f;
	stream->seek = bsdiff_stream_file_seek;
	stream->tell = bsdiff_stream_file_tell;
	if (!write) {
		stream->read = bsdiff_stream_file_read;
	} else {
		stream->write = bsdiff_stream_file_write;
		stream->flush = bsdiff_stream_file_flush;
	}
	stream->close = bsdiff_stream_file_close;

	return BSDIFF_SUCCESS;
}
