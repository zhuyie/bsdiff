#ifndef __BSDIFF_PRIVATE_H__
#define __BSDIFF_PRIVATE_H__

#define HANDLE_ERROR(errcode, fmt, ...) \
  do { \
    __bsdiff_log_error(ctx, errcode, fmt, ##__VA_ARGS__); \
    ret = errcode; \
    goto cleanup; \
  } while (0)

struct bsdiff_ctx;
void __bsdiff_log_error(struct bsdiff_ctx *ctx, int errcode, const char *fmt, ...);


int bsdiff_open_substream(
	struct bsdiff_stream *base,
	int64_t read_start,
	int64_t read_end,
	struct bsdiff_stream *substream);


/* bsdiff_compressor */
struct bsdiff_compressor
{
	void *state;
	int (*init)(void *state, struct bsdiff_stream *stream);
	int (*write)(void *state, const void *buffer, size_t size);
	int (*flush)(void *state);
	void (*close)(void *state);
};

void bsdiff_close_compressor(
	struct bsdiff_compressor *enc);


/* bsdiff_decompressor */
struct bsdiff_decompressor
{
	void *state;
	int (*init)(void *state, struct bsdiff_stream *stream);
	int (*read)(void *state, void *buffer, size_t size, size_t *readed);
	void (*close)(void *state);
};

void bsdiff_close_decompressor(
	struct bsdiff_decompressor *dec);

#endif /* !__BSDIFF_PRIVATE_H__ */
