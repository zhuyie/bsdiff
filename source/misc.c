#include <stdio.h>
#include <stdarg.h>
#include "misc.h"
#include "bsdiff.h"

void __bsdiff_log_error(struct bsdiff_ctx *ctx, int errcode, const char *fmt, ...)
{
	char buf[256] = { 0 };
	int n, off = 0, len = 255;
	va_list args;
	if (ctx->log_error) {
		n = snprintf(buf, len, "ERROR(%d): ", errcode);
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
