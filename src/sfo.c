
#include "../include/sfo.h"
#include <psp2/io/fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct __attribute__((packed)){uint32_t magic,version,key_off,data_off,count;} PsfHeader;
typedef struct __attribute__((packed)){uint16_t key_off,fmt;uint32_t len,max_len,data_off;} PsfEntry;

static int read_all(const char *path,uint8_t **out,size_t *sz){
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0); if(fd<0)return fd;
    SceOff end=sceIoLseek(fd,0,SCE_SEEK_END); if(end<=0||end>1024*1024){sceIoClose(fd);return -1;}
    sceIoLseek(fd,0,SCE_SEEK_SET); uint8_t *b=malloc((size_t)end);
    if(!b){sceIoClose(fd);return -2;} int n=sceIoRead(fd,b,(unsigned)end);sceIoClose(fd);
    if(n!=end){free(b);return -3;}*out=b;*sz=(size_t)end;return 0;
}
int mrw_sfo_get_string(const char *path,const char *key,char *out,size_t outsz){
    if(!out||!outsz)return -1;out[0]=0;uint8_t *b=NULL;size_t sz=0;int r=read_all(path,&b,&sz);
    if(r<0)return r;if(sz<sizeof(PsfHeader)){free(b);return -2;}PsfHeader *h=(PsfHeader*)b;
    if(h->magic!=0x46535000U){free(b);return -3;}
    if(sizeof(PsfHeader)+(size_t)h->count*sizeof(PsfEntry)>sz||h->key_off>=sz||h->data_off>=sz){free(b);return -4;}
    PsfEntry *e=(PsfEntry*)(b+sizeof(PsfHeader));
    for(uint32_t i=0;i<h->count;i++){
        size_t ko=(size_t)h->key_off+e[i].key_off,doff=(size_t)h->data_off+e[i].data_off;
        if(ko>=sz||doff>=sz)continue;const char *k=(char*)(b+ko);
        if(!memchr(k,0,sz-ko)||strcmp(k,key))continue;size_t n=e[i].len;
        if(n>sz-doff)n=sz-doff;if(n>=outsz)n=outsz-1;memcpy(out,b+doff,n);out[n]=0;free(b);return 0;
    }
    free(b);return -5;
}
