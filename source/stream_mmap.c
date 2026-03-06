#include "bsdiff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
#	include <windows.h>
#else
#	include <sys/mman.h>
#	include <fcntl.h>
#	include <unistd.h>
#endif

struct filestream_mmap_state
{
	void *addr;
	int64_t size;
	int64_t pos;
#if defined(_WIN32)
	HANDLE hFile;
	HANDLE hMap;
#else
	int fd;
#endif
};

static void mmapstream_close(void *state)
{
	struct filestream_mmap_state *s = (struct filestream_mmap_state *)state;
	if (!s) return;

#if defined(_WIN32)
	if (s->addr) UnmapViewOfFile(s->addr);
	if (s->hMap) CloseHandle(s->hMap);
	if (s->hFile != INVALID_HANDLE_VALUE) CloseHandle(s->hFile);
#else
	if (s->addr && s->addr != MAP_FAILED) munmap(s->addr, (size_t)s->size);
	if (s->fd != -1) close(s->fd);
#endif
	free(s);
}

static int mmapstream_getmode(void *state)
{
	return BSDIFF_MODE_READ;
}

static int mmapstream_seek(void *state, int64_t offset, int origin)
{
	struct filestream_mmap_state *s = (struct filestream_mmap_state *)state;
	int64_t new_pos;

	switch (origin) {
	case BSDIFF_SEEK_SET: new_pos = offset; break;
	case BSDIFF_SEEK_CUR: new_pos = s->pos + offset; break;
	case BSDIFF_SEEK_END: new_pos = s->size + offset; break;
	default: return BSDIFF_INVALID_ARG;
	}

	if (new_pos < 0 || new_pos > s->size)
		return BSDIFF_INVALID_ARG;

	s->pos = new_pos;
	return BSDIFF_SUCCESS;
}

static int mmapstream_tell(void *state, int64_t *position)
{
	struct filestream_mmap_state *s = (struct filestream_mmap_state *)state;
	*position = s->pos;
	return BSDIFF_SUCCESS;
}

static int mmapstream_read(void *state, void *buffer, size_t size, size_t *readed)
{
	struct filestream_mmap_state *s = (struct filestream_mmap_state *)state;
	size_t remain;

	*readed = 0;
	if (s->pos >= s->size)
		return BSDIFF_END_OF_FILE;

	remain = (size_t)(s->size - s->pos);
	if (size > remain)
		size = remain;

	memcpy(buffer, (uint8_t *)s->addr + s->pos, size);
	s->pos += (int64_t)size;
	*readed = size;

	return (size == 0 && remain > 0) ? BSDIFF_SUCCESS : (size > 0 ? BSDIFF_SUCCESS : BSDIFF_END_OF_FILE);
}

static int mmapstream_getbuffer(void *state, const void **ppbuffer, size_t *psize)
{
	struct filestream_mmap_state *s = (struct filestream_mmap_state *)state;
	*ppbuffer = s->addr;
	*psize = (size_t)s->size;
	return BSDIFF_SUCCESS;
}

int bsdiff_open_mmap_stream(
	int mode,
	const char *filename,
	struct bsdiff_stream *stream)
{
	struct filestream_mmap_state *s;
#if !defined(_WIN32)
	struct stat st;
#endif

	if (mode != BSDIFF_MODE_READ)
		return BSDIFF_INVALID_ARG;

	s = (struct filestream_mmap_state *)calloc(1, sizeof(*s));
	if (!s) return BSDIFF_OUT_OF_MEMORY;

#if defined(_WIN32)
	s->hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (s->hFile == INVALID_HANDLE_VALUE) {
		free(s);
		return BSDIFF_FILE_ERROR;
	}

	s->size = 0;
	{
		LARGE_INTEGER li;
		if (GetFileSizeEx(s->hFile, &li)) {
			s->size = li.QuadPart;
		}
	}

	if (s->size > 0) {
		s->hMap = CreateFileMapping(s->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!s->hMap) {
			CloseHandle(s->hFile);
			free(s);
			return BSDIFF_FILE_ERROR;
		}
		s->addr = MapViewOfFile(s->hMap, FILE_MAP_READ, 0, 0, 0);
		if (!s->addr) {
			CloseHandle(s->hMap);
			CloseHandle(s->hFile);
			free(s);
			return BSDIFF_FILE_ERROR;
		}
	}
#else
	s->fd = open(filename, O_RDONLY);
	if (s->fd == -1) {
		free(s);
		return BSDIFF_FILE_ERROR;
	}

	if (fstat(s->fd, &st) == -1) {
		close(s->fd);
		free(s);
		return BSDIFF_FILE_ERROR;
	}
	s->size = st.st_size;

	if (s->size > 0) {
		s->addr = mmap(NULL, (size_t)s->size, PROT_READ, MAP_PRIVATE, s->fd, 0);
		if (s->addr == MAP_FAILED) {
			close(s->fd);
			free(s);
			return BSDIFF_FILE_ERROR;
		}
	}
#endif

	memset(stream, 0, sizeof(*stream));
	stream->state = s;
	stream->close = mmapstream_close;
	stream->get_mode = mmapstream_getmode;
	stream->seek = mmapstream_seek;
	stream->tell = mmapstream_tell;
	stream->read = mmapstream_read;
	stream->get_buffer = mmapstream_getbuffer;

	return BSDIFF_SUCCESS;
}
