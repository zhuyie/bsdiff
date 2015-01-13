#include "bsdiff_misc.h"
#include <limits.h>
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
    long long value = 0;

    value = buf[7] & 0x7F;
    value *= 256;  value += buf[6];
    value *= 256;  value += buf[5];
    value *= 256;  value += buf[4];
    value *= 256;  value += buf[3];
    value *= 256;  value += buf[2];
    value *= 256;  value += buf[1];
    value *= 256;  value += buf[0];
    if (buf[7] & 0x80)
        value = -value;

    assert(value >= INT_MIN && value <= INT_MAX);  // 当前实现中应该在int范围之内
    return (int)value;
}

void bsdiff_WriteOffset(int offset, unsigned char buf[8])
{
    long long value = offset;

    if (offset < 0)
        value = -value;

    buf[0] = value % 256;  value -= buf[0];  value /= 256;
    buf[1] = value % 256;  value -= buf[1];  value /= 256;
    buf[2] = value % 256;  value -= buf[2];  value /= 256;
    buf[3] = value % 256;  value -= buf[3];  value /= 256;
    buf[4] = value % 256;  value -= buf[4];  value /= 256;
    buf[5] = value % 256;  value -= buf[5];  value /= 256;
    buf[6] = value % 256;  value -= buf[6];  value /= 256;
    buf[7] = (unsigned char)value;

    if (offset < 0)
        buf[7] |= 0x80;
}

void bsdiff_SetError(char error[64], const char *str)
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
