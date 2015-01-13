#ifndef __BSDIFF_MISC_H__
#define __BSDIFF_MISC_H__

#include <stdio.h>

//------------------------------------------------------------------------------

int bsdiff_ReadFile(
    FILE *fp, 
    unsigned char *buf, 
    size_t len
    );

int bsdiff_WriteFile(
    FILE *fp, 
    const unsigned char *buf, 
    size_t len
    );

int bsdiff_GetFileSize(
    FILE *fp, 
    int *fileSize
    );

int bsdiff_ReadOffset(
    unsigned char buf[8]
    );

void bsdiff_WriteOffset(
    int offset,
    unsigned char buf[8]
    );

void bsdiff_WriteError(
    char error[64], 
    const char *str
    );

//------------------------------------------------------------------------------

#endif // !__BSDIFF_MISC_H__
