#include "bsdiff_misc.h"
#include <assert.h>

//------------------------------------------------------------------------------

int bsdiff_ReadFile(FILE *fp, unsigned char *buf, size_t len)
{
    size_t n;

    while (len) {
        n = fread(buf, 1, len, fp);
        if (!n)
            return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

int bsdiff_WriteFile(FILE *fp, const unsigned char *buf, size_t len)
{
    size_t n;
    
    while (len) {
        n = fwrite(buf, 1, len, fp);
        if (!n)
            return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

int bsdiff_GetFileSize(FILE *fp, int *fileSize)
{
    if (fseek(fp, 0, SEEK_END))
        return 0;
    
    *fileSize = ftell(fp);
    if (*fileSize < 0)
        return 0;
    
    if (fseek(fp, 0, SEEK_SET))
        return 0;
    
    return 1;
}

int bsdiff_ReadOffset(unsigned char buf[8])
{
    unsigned int off = 0;

    if (buf[7] || buf[6] || buf[5] || buf[4])
        return -1;

    off += buf[3];
    off = off * 256; off += buf[2];
    off = off * 256; off += buf[1];
    off = off * 256; off += buf[0];
    if (off > 0x7fffffff)
        return -1;

    return (int)off;
}

void bsdiff_WriteOffset(int offset, unsigned char buf[8])
{
    assert(offset >= 0);

                    buf[0] = offset % 256;  offset -= buf[0];
    offset /= 256;  buf[1] = offset % 256;  offset -= buf[1];
    offset /= 256;  buf[2] = offset % 256;  offset -= buf[2];
    offset /= 256;  buf[3] = offset % 256;  offset -= buf[3];

    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;
}

void bsdiff_WriteError(char error[64], const char *str)
{
    int i;
    
    if (error) {
        for (i = 0; i < 63; ++i) {
            if (!str[i])
                break;
            error[i] = str[i];
        }
        error[i] = '\0';
    }
}

//------------------------------------------------------------------------------
