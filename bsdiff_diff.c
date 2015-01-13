#include "bsdiff_diff.h"
#include "bsdiff_misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "bzlib.h"

//------------------------------------------------------------------------------

typedef unsigned char u_char;

static void qsufsort(
    off_t *I,
    off_t *V,
    u_char *old,
    off_t oldsize
    );
static off_t search(
	off_t *I, 
	u_char *old, 
	off_t oldsize,
	u_char *_new, 
	off_t newsize, 
	off_t st, 
	off_t en, 
	off_t *pos
	);

//------------------------------------------------------------------------------

int bsdiff_diff(const char *oldFile, const char *newFile, const char *patchFile, char error[64])
{
    int retCode = 0;
    FILE *fp = NULL;
    BZFILE *bfp = NULL;
    unsigned char *oldFileBuf = NULL, *newFileBuf = NULL;
    off_t *I, *V;
    unsigned char *diffBlock = NULL, *extraBlock = NULL;
    int oldSize, newSize, diffBlockLen, extraBlockLen;
    unsigned char header[32], ctrl[24];
    int bzError;
    off_t scan, pos, len, len2;
    off_t lastscan, lastpos, lastoffset;
    off_t oldscore, scsc;
    off_t s, Sf, lenf, Sb, lenb;
    off_t overlap, Ss, lens;
    off_t i;

    // 打开oldFile，将其内容读入oldFileBuf
    if (!(fp = fopen(oldFile, "rb")) || !bsdiff_GetFileSize(fp, &oldSize)) {
        bsdiff_SetError(error, "Can't open oldFile");
        goto MyExit;
    }
    if (!(oldFileBuf = (unsigned char*)malloc(oldSize + 1))) {
        bsdiff_SetError(error, "Out of memory");
        goto MyExit;
    }
    if (!bsdiff_ReadFile(fp, oldFileBuf, oldSize)) {
        bsdiff_SetError(error, "Can't read oldFile");
        goto MyExit;
    }
    fclose(fp);
    fp = NULL;

    // 分配两个buffer（I和V），其尺寸为(oldSize + 1) * sizeof(off_t)
    I = (off_t*)malloc((oldSize + 1) * sizeof(off_t));
    V = (off_t*)malloc((oldSize + 1) * sizeof(off_t));
    if (!I || !V) {
        bsdiff_SetError(error, "Out of memory");
        goto MyExit;
    }

    // 执行qsufsort
    qsufsort(I, V, oldFileBuf, oldSize);

    // V现在不用了，可以释放掉了
    free(V);
    V = NULL;

    // 打开newFile，将其内容读入newFileBuf
    if (!(fp = fopen(newFile, "rb")) || !bsdiff_GetFileSize(fp, &newSize)) {
        bsdiff_SetError(error, "Can't open newFile");
        goto MyExit;
    }
    if (!(newFileBuf = (unsigned char*)malloc(newSize + 1))) {
        bsdiff_SetError(error, "Out of memory");
        goto MyExit;
    }
    if (!bsdiff_ReadFile(fp, newFileBuf, newSize)) {
        bsdiff_SetError(error, "Can't read newFile");
        goto MyExit;
    }
    fclose(fp);
    fp = NULL;

    // 分配两个buffer（diffBlock和extraBlock），其尺寸为(newSize + 1)
    diffBlock = (unsigned char*)malloc(newSize + 1);
    extraBlock = (unsigned char*)malloc(newSize + 1);
    if (!diffBlock || !extraBlock) {
        bsdiff_SetError(error, "Out of memory");
        goto MyExit;
    }

    diffBlockLen = 0;
    extraBlockLen = 0;

    // 创建（打开）patchFile
    if (!(fp = fopen(patchFile, "wb"))) {
        bsdiff_SetError(error, "Can't open patchFile");
        goto MyExit;
    }

    // 写出文件头（占位，后面还要更新）
    memcpy(header, "BSDIFF40", 8);
    bsdiff_WriteOffset(0, header + 8);
    bsdiff_WriteOffset(0, header + 16);
    bsdiff_WriteOffset(newSize, header + 24);
    if (!bsdiff_WriteFile(fp, header, 32)) {
        bsdiff_SetError(error, "Can't write patchFile");
        goto MyExit;
    }

    // 紧跟着header的是BZ2(ctrl block)
    if (!(bfp = BZ2_bzWriteOpen(&bzError, fp, 9, 0, 0))) {
        bsdiff_SetError(error, "BZ2_bzWriteOpen failed");
        goto MyExit;
    }

	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newSize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newSize;scan++) {
			len=search(I, oldFileBuf, oldSize, newFileBuf + scan, newSize - scan,
					0, oldSize, &pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldSize) &&
				(oldFileBuf[scsc+lastoffset] == newFileBuf[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldSize) &&
				(oldFileBuf[scan+lastoffset] == newFileBuf[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newSize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldSize);) {
				if(oldFileBuf[lastpos+i]==newFileBuf[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newSize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(oldFileBuf[pos-i]==newFileBuf[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(newFileBuf[lastscan+lenf-overlap+i]==
					   oldFileBuf[lastpos+lenf-overlap+i]) s++;
					if(newFileBuf[scan-lenb+i]==
					   oldFileBuf[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				diffBlock[diffBlockLen+i]=newFileBuf[lastscan+i]-oldFileBuf[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				extraBlock[extraBlockLen+i]=newFileBuf[lastscan+lenf+i];

			diffBlockLen+=lenf;
			extraBlockLen+=(scan-lenb)-(lastscan+lenf);

			// 写出一组ctrl data
			bsdiff_WriteOffset(lenf, ctrl);
			bsdiff_WriteOffset((scan-lenb)-(lastscan+lenf), ctrl + 8);
			bsdiff_WriteOffset((pos-lenb)-(lastpos+lenf), ctrl + 16);
			BZ2_bzWrite(&bzError, bfp, ctrl, 24);
			if (bzError != BZ_OK) { 
				bsdiff_SetError(error, "BZ2_bzWrite failed");
				goto MyExit;
			}

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};
    BZ2_bzWriteClose(&bzError, bfp, 0, NULL, NULL);
    if (bzError != BZ_OK) {
        bsdiff_SetError(error, "BZ2_bzWriteClose failed");
        goto MyExit;
    }
    bfp = NULL;

    // 取得BZ2(ctrl block)的长度，填回到header中去
    if ((len = ftell(fp)) == -1) {
        bsdiff_SetError(error, "ftell failed");
        goto MyExit;
    }
    bsdiff_WriteOffset(len-32, header + 8);

    // 写BZ2(diff block)
    if ((bfp = BZ2_bzWriteOpen(&bzError, fp, 9, 0, 0)) == NULL) {
        bsdiff_SetError(error, "BZ2_bzWriteOpen failed");
        goto MyExit;
    }
    BZ2_bzWrite(&bzError, bfp, diffBlock, diffBlockLen);
    if (bzError != BZ_OK) {
        bsdiff_SetError(error, "BZ2_bzWriteClose failed");
        goto MyExit;
    }
    BZ2_bzWriteClose(&bzError, bfp, 0, NULL, NULL);
    if (bzError != BZ_OK) {
        bsdiff_SetError(error, "BZ2_bzWriteClose failed");
        goto MyExit;
    }
    bfp = NULL;

    // 取得BZ2(diff block)的长度，填回到header中去
    if ((len2 = ftell(fp)) == -1) {
        bsdiff_SetError(error, "ftell failed");
        goto MyExit;
    }
    bsdiff_WriteOffset(len2 - len, header + 16);

    // 写BZ2(extra block)
    if ((bfp = BZ2_bzWriteOpen(&bzError, fp, 9, 0, 0)) == NULL) {
        bsdiff_SetError(error, "BZ2_bzWriteOpen failed");
        goto MyExit;
    }
    BZ2_bzWrite(&bzError, bfp, extraBlock, extraBlockLen);
    if (bzError != BZ_OK) {
        bsdiff_SetError(error, "BZ2_bzWriteClose failed");
        goto MyExit;
    }
    BZ2_bzWriteClose(&bzError, bfp, 0, NULL, NULL);
    if (bzError != BZ_OK) {
        bsdiff_SetError(error, "BZ2_bzWriteClose failed");
        goto MyExit;
    }
    bfp = NULL;

    // Seek to the beginning, write the header, and close the file
    if (fseek(fp, 0, SEEK_SET) || fwrite(header, 32, 1, fp) != 1 || fclose(fp)) {
        bsdiff_SetError(error, "failed to update header");
        goto MyExit;
    }
    fp = NULL;

    retCode = 1;

MyExit:
    free(oldFileBuf);
    free(newFileBuf);
    free(I);
    free(V);
    free(diffBlock);
    free(extraBlock);
    if (fp)
        fclose(fp);
    return retCode;
}

//------------------------------------------------------------------------------

static void split(off_t *I, off_t *V, off_t start, off_t len, off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(off_t *I,off_t *V, u_char *old, off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static off_t matchlen(u_char *old, off_t oldsize, u_char *_new, off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=_new[i]) break;

	return i;
}

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static off_t search(off_t *I, u_char *old, off_t oldsize,
		u_char *_new, off_t newsize, off_t st, off_t en, off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],_new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],_new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],_new,MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,_new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,_new,newsize,st,x,pos);
	};
}

//------------------------------------------------------------------------------

// #define BSDIFF_STANDALONE

#ifdef BSDIFF_STANDALONE

int main(int argc,char * argv[])
{
    char error[64];

    if (argc != 4) {
        printf("usage: %s oldFile newFile patchFile\n", argv[0]);
        return 1;
    }

    if (!bsdiff_diff(argv[1], argv[2], argv[3], error)) {
        printf("diff failed! error = %s\n", error);
        return 1;
    }
    printf("diff OK\n");
    return 0;
}

#endif // BSDIFF_STANDALONE
