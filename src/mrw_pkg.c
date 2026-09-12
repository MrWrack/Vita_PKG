
#include "../include/mrw_pkg.h"
#include "../include/sha256.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char MRW_MAGIC[8] = {'M','R','W','P','K','G','1',0};
#define MAX_PATH_LEN 480
#define COPY_BUF 65536

static uint32_t rd32(SceUID fd, int *ok){
    uint8_t b[4]; if(sceIoRead(fd,b,4)!=4){*ok=0;return 0;}
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
static uint64_t rd64(SceUID fd, int *ok){
    uint8_t b[8]; if(sceIoRead(fd,b,8)!=8){*ok=0;return 0;}
    uint64_t v=0; for(int i=7;i>=0;i--) v=(v<<8)|b[i]; return v;
}
static int safe_rel(const char *p){
    if(!p||!*p||p[0]=='/'||p[0]=='\\'||strchr(p,':'))return 0;
    if(!strcmp(p,"..")||!strncmp(p,"../",3)||strstr(p,"/../")||strstr(p,"\\..\\"))return 0;
    return 1;
}
static void ensure_parent_dirs(const char *path){
    char tmp[MAX_PATH_LEN]; snprintf(tmp,sizeof(tmp),"%s",path);
    for(char *p=tmp+1;*p;p++){ if(*p=='/'){ *p=0; sceIoMkdir(tmp,0777); *p='/'; } }
}
static int hashes_equal(const uint8_t a[32],const uint8_t b[32]){
    unsigned x=0;for(int i=0;i<32;i++)x|=(unsigned)(a[i]^b[i]);return x==0;
}
int mrw_pkg_extract_v2(const char *pkg_path,const char *dest_root,MrwInstallProgress *pr){
    if(pr){pr->percent=0;snprintf(pr->stage,sizeof(pr->stage),"Read");snprintf(pr->message,sizeof(pr->message),"Opening MRW-PKG");}
    SceUID fd=sceIoOpen(pkg_path,SCE_O_RDONLY,0); if(fd<0)return fd;
    char magic[8]={0}; if(sceIoRead(fd,magic,8)!=8||memcmp(magic,MRW_MAGIC,8)){sceIoClose(fd);return -100;}
    int ok=1; uint32_t ver=rd32(fd,&ok); if(!ok||ver!=2){sceIoClose(fd);return -101;}
    uint32_t manifest_len=rd32(fd,&ok); if(!ok||manifest_len>1024*1024){sceIoClose(fd);return -102;}
    if(manifest_len){ if(sceIoLseek(fd,manifest_len,SCE_SEEK_CUR)<0){sceIoClose(fd);return -103;} }
    uint32_t count=rd32(fd,&ok); if(!ok||count>10000){sceIoClose(fd);return -104;}

    uint8_t *buf=malloc(COPY_BUF); if(!buf){sceIoClose(fd);return -105;}
    for(uint32_t i=0;i<count;i++){
        uint32_t nlen=rd32(fd,&ok); if(!ok||nlen==0||nlen>=MAX_PATH_LEN){free(buf);sceIoClose(fd);return -106;}
        char rel[MAX_PATH_LEN]; if(sceIoRead(fd,rel,nlen)!=(int)nlen){free(buf);sceIoClose(fd);return -107;}
        rel[nlen]=0; if(!safe_rel(rel)){free(buf);sceIoClose(fd);return -108;}
        uint64_t size=rd64(fd,&ok); if(!ok){free(buf);sceIoClose(fd);return -109;}
        uint8_t expected[32],actual[32]; if(sceIoRead(fd,expected,32)!=32){free(buf);sceIoClose(fd);return -110;}

        char outp[MAX_PATH_LEN]; snprintf(outp,sizeof(outp),"%s/%s",dest_root,rel); ensure_parent_dirs(outp);
        SceUID out=sceIoOpen(outp,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666); if(out<0){free(buf);sceIoClose(fd);return -111;}

        MrwSha256 sha; mrw_sha256_init(&sha); uint64_t remain=size;
        while(remain){
            unsigned chunk=(unsigned)(remain>COPY_BUF?COPY_BUF:remain);
            int got=sceIoRead(fd,buf,chunk); if(got!=(int)chunk){sceIoClose(out);free(buf);sceIoClose(fd);return -112;}
            if(sceIoWrite(out,buf,chunk)!=(int)chunk){sceIoClose(out);free(buf);sceIoClose(fd);return -113;}
            mrw_sha256_update(&sha,buf,chunk); remain-=chunk;
        }
        sceIoClose(out); mrw_sha256_final(&sha,actual);
        if(!hashes_equal(expected,actual)){free(buf);sceIoClose(fd);return -114;}

        if(pr){pr->percent=(int)(((i+1)*75U)/(count?count:1));snprintf(pr->stage,sizeof(pr->stage),"Verify");
            snprintf(pr->message,sizeof(pr->message),"%u/%u files verified",(unsigned)(i+1),(unsigned)count);}
    }
    free(buf);sceIoClose(fd); if(pr){pr->percent=75;snprintf(pr->stage,sizeof(pr->stage),"Extract");snprintf(pr->message,sizeof(pr->message),"Package extracted");}
    return 0;
}
