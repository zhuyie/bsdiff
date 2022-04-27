#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "bsdiff.h"
#include "bsdiff_private.h"

static const char* err_code_str(int errcode)
{
	switch (errcode) {
	case BSDIFF_SUCCESS:
		return "success";
	case BSDIFF_ERROR:
		return "generic error";
	case BSDIFF_INVALID_ARG:
		return "invalid argument";
	case BSDIFF_OUT_OF_MEMORY:
		return "out of memory";
	case BSDIFF_FILE_ERROR:
		return "file related error";
	case BSDIFF_END_OF_FILE:
		return "end of file";
	case BSDIFF_CORRUPT_PATCH:
		return "corrupt patch data";
	case BSDIFF_SIZE_TOO_LARGE:
		return "size is too large";
	default:
		return "unknown error";
	}
}

void __bsdiff_log_error(struct bsdiff_ctx *ctx, int errcode, const char *fmt, ...)
{
	char buf[256] = { 0 };
	int n, off = 0, len = 255;
	va_list args;
	if (ctx->log_error) {
		n = snprintf(buf, len, "bsdiff err: %s(%d), msg: ", err_code_str(errcode), errcode);
		if (n < 0)
			return;
		if (n >= len) {
			off += len - 1;
			goto do_output;
		}
		off += n; len -= n;

		va_start(args, fmt);
		n = vsnprintf(buf + off, len, fmt, args);
		va_end(args);
		if (n < 0)
			return;
		if (n >= len) {
			off += len - 1;
			goto do_output;
		}
		off += n; len -= n;

do_output:
		buf[off] = '\n';
		ctx->log_error(ctx->opaque, buf);
	}
}

void bsdiff_close_stream(
	struct bsdiff_stream *stream)
{
	if (stream->close != NULL) {
		stream->close(stream->state);
		memset(stream, 0, sizeof(*stream));
	}
}

void bsdiff_close_compressor(
	struct bsdiff_compressor *enc)
{
	if (enc->close != NULL) {
		enc->close(enc->state);
		memset(enc, 0, sizeof(*enc));
	}
}

void bsdiff_close_decompressor(
	struct bsdiff_decompressor *dec)
{
	if (dec->close != NULL) {
		dec->close(dec->state);
		memset(dec, 0, sizeof(*dec));
	}
}

void bsdiff_close_patch_packer(
	struct bsdiff_patch_packer *packer)
{
	if (packer->close != NULL) {
		packer->close(packer->state);
		memset(packer, 0, sizeof(*packer));
	}
}
