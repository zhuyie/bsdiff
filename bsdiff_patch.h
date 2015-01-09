#ifndef __BSDIFF_PATCH_H__
#define __BSDIFF_PATCH_H__

#ifdef __cplusplus
extern "C" {
#endif

int bsdiff_patch(
    const char *oldFile, 
    const char *patchFile, 
    const char *newFile, 
    char error[64]
    );

#ifdef __cplusplus
}
#endif

#endif // !__BSDIFF_PATCH_H__
