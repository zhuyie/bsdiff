#ifndef __BSDIFF_SUBSTREAM_H__
#define __BSDIFF_SUBSTREAM_H__

#include "bsdiff.h"

int bsdiff_open_substream(
	struct bsdiff_stream *base,
	int64_t read_start,
	int64_t read_end,
	struct bsdiff_stream *substream);

#endif /* !__BSDIFF_SUBSTREAM_H__ */
