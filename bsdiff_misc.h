#ifndef __BSDIFF_MISC_H__
#define __BSDIFF_MISC_H__

#include <stdio.h>

//------------------------------------------------------------------------------

int bsdiffReadFile(
    FILE *fp, 
    unsigned char *buf, 
    size_t len
    );

int bsdiffWriteFile(
    FILE *fp, 
    const unsigned char *buf, 
    size_t len
    );

int bsdiffGetFileSize(
    FILE *fp, 
    int *fileSize
    );

int bsdiffReadOffset(
    unsigned char buf[8]
    );

void bsdiffWriteError(
    char error[64], 
    const char *str
    );

//------------------------------------------------------------------------------

#endif // !__BSDIFF_MISC_H__
