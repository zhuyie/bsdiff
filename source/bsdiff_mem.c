#include "bsdiff.h"
#include "bsdiff_mem.h"
#include <stdlib.h>
#include <string.h>

/*
 * Memory tracking implementation.
 *
 * Each allocation prepends a size_t header storing the usable size.
 * Layout: [size_t: size][user data ...]
 *                       ^ returned pointer
 *
 * This allows bsdiff_free to know how many bytes are being freed,
 * and bsdiff_realloc to adjust the delta correctly.
 *
 * Global stats are maintained in a static struct (not thread-safe,
 * consistent with bsdiff's single-threaded design).
 */

static struct bsdiff_mem_stats g_mem_stats;

void *bsdiff_malloc(size_t size)
{
	void *raw;
	size_t *header;

	if (size == 0)
		size = 1;

	raw = malloc(sizeof(size_t) + size);
	if (raw == NULL)
		return NULL;

	header = (size_t *)raw;
	*header = size;

	g_mem_stats.current_bytes += (int64_t)size;
	if (g_mem_stats.current_bytes > g_mem_stats.peak_bytes)
		g_mem_stats.peak_bytes = g_mem_stats.current_bytes;
	g_mem_stats.total_allocs++;

	return (void *)(header + 1);
}

void *bsdiff_realloc(void *ptr, size_t size)
{
	void *raw, *newraw;
	size_t *header;
	size_t old_size;

	if (ptr == NULL)
		return bsdiff_malloc(size);

	if (size == 0) {
		bsdiff_free(ptr);
		return NULL;
	}

	header = ((size_t *)ptr) - 1;
	old_size = *header;
	raw = (void *)header;

	newraw = realloc(raw, sizeof(size_t) + size);
	if (newraw == NULL)
		return NULL;

	header = (size_t *)newraw;
	*header = size;

	g_mem_stats.current_bytes += (int64_t)size - (int64_t)old_size;
	if (g_mem_stats.current_bytes > g_mem_stats.peak_bytes)
		g_mem_stats.peak_bytes = g_mem_stats.current_bytes;

	return (void *)(header + 1);
}

void bsdiff_free(void *ptr)
{
	size_t *header;
	size_t size;

	if (ptr == NULL)
		return;

	header = ((size_t *)ptr) - 1;
	size = *header;

	g_mem_stats.current_bytes -= (int64_t)size;
	g_mem_stats.total_frees++;

	free((void *)header);
}

void bsdiff_get_mem_stats(struct bsdiff_mem_stats *stats)
{
	if (stats != NULL)
		*stats = g_mem_stats;
}

void bsdiff_reset_mem_stats(void)
{
	memset(&g_mem_stats, 0, sizeof(g_mem_stats));
}
