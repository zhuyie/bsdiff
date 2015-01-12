#include "bsdiff_patch.h"
#include "bsdiff_misc.h"
#include <stdlib.h>
#include <stdio.h>
#include "bzlib.h"

//------------------------------------------------------------------------------

static int readHeader(
    FILE *fp, 
    int *controlBlockSize, 
    int *diffBlockSize, 
    int *newFileSize
    );

//------------------------------------------------------------------------------

int bsdiff_patch(const char *oldFile, const char *patchFile, const char *newFile, char error[64])
{
    int retCode = 0;
    FILE *fp = NULL, *fpControl = NULL, *fpDiff = NULL, *fpExtra = NULL;
    BZFILE *bfpControl = NULL, *bfpDiff = NULL, *bfpExtra = NULL;
    unsigned char *oldFileBuf = NULL, *newFileBuf = NULL;
    int controlBlockSize, diffBlockSize, newFileSize, oldFileSize;
    int oldPos, newPos;
    int bzError, bzReaded;
    int i, cb, ctrl[3];
    unsigned char temp[24];

    /* 文件格式描述如下：
       offset  len
        0       8   --> "BSDIFF40"
        8       8   --> X, length of the bzip2(control block)
        16      8   --> Y, length of the bzip2(diff block)
        24      8   --> newfile size
        32      X   --> bzip2(control block)
        32+X    Y   --> bzip2(diff block)
        32+X+Y  ?   --> bzip2(extra block)

        其中control block是一系列的3元组(x,y,z)，xyz均为8字节的无符号整数，其含义为：
        add x bytes from oldfile to x bytes from the diff block;
        copy y bytes from the extra block;
        seek forwards in oldfile by z bytes;

       限制：
       在当前实现中，采用了一次性分配内存buffer的方式来处理。由于32位进程的2GB用户态地址空间的限制，
       无法分配出超过2GB的buffer，因此在这个版本中不支持大于2GB的文件。
    */

    // 打开patch文件，读取并校验文件头
    fp = fopen(patchFile, "rb");
    if (!fp) {
        bsdiffWriteError(error, "Can't open patchFile");
        goto MyExit;
    }
    if (!readHeader(fp, &controlBlockSize, &diffBlockSize, &newFileSize)) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }

    // 打开三个BZ2的文件句柄，分别读取patch文件的三个部分
    fpControl = fp;  // fp的pos应该刚好就在32上面
    fp = NULL;
    bfpControl = BZ2_bzReadOpen(&bzError, fpControl, 0, 0, NULL, 0);
    if (!bfpControl) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }

    fpDiff = fopen(patchFile, "rb");
    if (!fpDiff || fseek(fpDiff, 32 + controlBlockSize, SEEK_SET)) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }
    bfpDiff = BZ2_bzReadOpen(&bzError, fpDiff, 0, 0, NULL, 0);
    if (!bfpDiff) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }

    fpExtra = fopen(patchFile, "rb");
    if (!fpExtra || fseek(fpExtra, 32 + controlBlockSize + diffBlockSize, SEEK_SET)) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }
    bfpExtra = BZ2_bzReadOpen(&bzError, fpExtra, 0, 0, NULL, 0);
    if (!bfpDiff) {
        bsdiffWriteError(error, "Invalid patchFile");
        goto MyExit;
    }

    // 读取oldFile内容到oldFileBuf
    fp = fopen(oldFile, "rb");
    if (!fp || !bsdiffGetFileSize(fp, &oldFileSize)) {
        bsdiffWriteError(error, "Can't open oldFile");
        goto MyExit;
    }
    oldFileBuf = (unsigned char*)malloc(oldFileSize);
    if (!oldFileBuf) {
        bsdiffWriteError(error, "Out of memory");
        goto MyExit;
    }
    if (!bsdiffReadFile(fp, oldFileBuf, oldFileSize)) {
        bsdiffWriteError(error, "Failed to read oldFile");
        goto MyExit;
    }
    fclose(fp);
    fp = NULL;

    // 分配newFileBuf
    newFileBuf = (unsigned char*)malloc(newFileSize);
    if (!newFileBuf) {
        bsdiffWriteError(error, "Out of memory");
        goto MyExit;
    }

    // 开始循环处理
    oldPos = 0;
    newPos = 0;
    while (newPos < newFileSize) {
        // 读Control data
        bzReaded = BZ2_bzRead(&bzError, bfpControl, temp, 24);
        if ((bzError != BZ_OK && bzError != BZ_STREAM_END) || bzReaded != 24) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }
        ctrl[0] = bsdiffReadOffset(temp);
        ctrl[1] = bsdiffReadOffset(temp + 8);
        ctrl[2] = bsdiffReadOffset(temp + 16);
        
        // 从diff数据中读ctrl[0]个字节
        if (ctrl[0] < 0 || newPos + ctrl[0] > newFileSize) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }
        bzReaded = BZ2_bzRead(&bzError, bfpDiff, newFileBuf + newPos, ctrl[0]);
        if ((bzError != BZ_OK && bzError != BZ_STREAM_END) || bzReaded != ctrl[0]) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }

        // 从旧文件数据中读取ctrl[0]个字节，与diff数据进行加操作
        cb = ctrl[0];
        if (oldPos + cb > oldFileSize)
            cb = oldFileSize - oldPos;
        for (i = 0; i < cb; ++i) {
            newFileBuf[newPos + i] += oldFileBuf[oldPos + i];
        }

        // 调整pos
        newPos += ctrl[0];
        oldPos += ctrl[0];

        // 从extra数据中读取ctrl[1]个字节
        if (ctrl[1] < 0 || newPos + ctrl[1] > newFileSize) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }
        bzReaded = BZ2_bzRead(&bzError, bfpExtra, newFileBuf + newPos, ctrl[1]);
        if ((bzError != BZ_OK && bzError != BZ_STREAM_END) || bzReaded != ctrl[1]) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }

        // 调整pos
        if (ctrl[2] < 0) {
            bsdiffWriteError(error, "Invalid patchFile");
            goto MyExit;
        }
        newPos += ctrl[1];
        oldPos += ctrl[2];
    }

    // 将newFileBuf中的内容写出到newFile
    fp = fopen(newFile, "wb");
    if (!fp) {
        bsdiffWriteError(error, "Can't open newFile");
        goto MyExit;
    }
    if (!bsdiffWriteFile(fp, newFileBuf, newFileSize)) {
        bsdiffWriteError(error, "Failed to write newFile");
        goto MyExit;
    }
    fclose(fp);
    fp = NULL;

    // Done
    retCode = 1;

MyExit:
    if (oldFileBuf)
        free(oldFileBuf);
    if (newFileBuf)
        free(newFileBuf);
    if (bfpControl)
        BZ2_bzReadClose(&bzError, bfpControl);
    if (bfpDiff)
        BZ2_bzReadClose(&bzError, bfpDiff);
    if (bfpExtra)
        BZ2_bzReadClose(&bzError, bfpExtra);
    if (fp)
        fclose(fp);
    if (fpControl)
        fclose(fpControl);
    if (fpDiff)
        fclose(fpDiff);
    if (fpExtra)
        fclose(fpExtra);
    return retCode;
}

//------------------------------------------------------------------------------

static int readHeader(
    FILE *fp, 
    int *controlBlockSize, 
    int *diffBlockSize, 
    int *newFileSize
    )
{
    unsigned char header[32];

    if (fread(header, 1, 32, fp) != 32)
        return 0;
    
    if (memcmp(header, "BSDIFF40", 8) != 0)
        return 0;

    *controlBlockSize = bsdiffReadOffset(header + 8);
    *diffBlockSize = bsdiffReadOffset(header + 16);
    *newFileSize = bsdiffReadOffset(header + 24);
    if (*controlBlockSize < 0 || *diffBlockSize < 0 || *newFileSize < 0)
        return 0;

    return 1;
}

//------------------------------------------------------------------------------

// #define TEST_BSDIFF

#ifdef TEST_BSDIFF

int main(int argc,char * argv[])
{
    char error[64];

    if (argc != 4) {
        printf("usage: %s oldfile newfile patchfile\n", argv[0]);
        return 1;
    }

    if (!bsdiff_patch(argv[1], argv[3], argv[2], error)) {
        printf("patch failed! error = %s\n", error);
        return 1;
    }
    printf("patch OK\n");
    return 0;
}

#endif // TEST_BSDIFF
