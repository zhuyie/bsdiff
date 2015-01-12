#ifndef __BSDIFF_DIFF_H__
#define __BSDIFF_DIFF_H__

#ifdef __cplusplus
extern "C" {
#endif

int bsdiff_diff(
    const char *oldFile, 
    const char *newFile, 
    const char *patchFile, 
    char error[64]
    );

#ifdef __cplusplus
}
#endif

#endif // !__BSDIFF_DIFF_H__
