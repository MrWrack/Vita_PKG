
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/app.h"
#include "../include/sha1.h"
#include "../include/sfo.h"

#define PACKAGE_TEMP "ux0:data/MrWrackPKG/package_temp"
#define PARAM_SFO PACKAGE_TEMP "/sce_sys/param.sfo"
#define HEAD_BIN PACKAGE_TEMP "/sce_sys/package/head.bin"
#define HEAD_TEMPLATE "app0:resources/head.bin"

static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int exists(const char *p){SceIoStat s;memset(&s,0,sizeof(s));return sceIoGetstat(p,&s)>=0;}

static int read_file(const char *p,uint8_t **out,size_t *sz){
    SceUID fd=sceIoOpen(p,SCE_O_RDONLY,0);if(fd<0)return fd;SceOff end=sceIoLseek(fd,0,SCE_SEEK_END);
    if(end<=0||end>16*1024*1024){sceIoClose(fd);return -1;}sceIoLseek(fd,0,SCE_SEEK_SET);
    uint8_t *b=malloc((size_t)end);if(!b){sceIoClose(fd);return -2;}int n=sceIoRead(fd,b,(unsigned)end);sceIoClose(fd);
    if(n!=end){free(b);return -3;}*out=b;*sz=(size_t)end;return 0;
}
static int write_file(const char *p,const uint8_t *b,size_t sz){
    SceUID fd=sceIoOpen(p,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);if(fd<0)return fd;
    int n=sceIoWrite(fd,b,(unsigned)sz);sceIoClose(fd);return n==(int)sz?0:-1;
}
static void fpkg_hmac(const uint8_t *data,unsigned len,uint8_t out[16]){
    MrwSha1 c;uint8_t s[20],b[64];mrw_sha1_init(&c);mrw_sha1_update(&c,data,len);mrw_sha1_final(&c,s);
    memset(b,0,64);memcpy(b,s+4,8);memcpy(b+8,s+4,8);memcpy(b+16,s+12,4);
    b[20]=s[16];b[21]=s[1];b[22]=s[2];b[23]=s[3];memcpy(b+24,b+16,8);
    mrw_sha1_init(&c);mrw_sha1_update(&c,b,64);mrw_sha1_final(&c,s);memcpy(out,s,16);
}
static int valid_tid(const char *s){
    if(!s||strlen(s)!=9)return 0;for(int i=0;i<9;i++){unsigned char c=s[i];if(!(isdigit(c)||(c>='A'&&c<='Z')))return 0;}return 1;
}
int mrw_validate_package_temp(void){
    if(!exists(PACKAGE_TEMP "/eboot.bin"))return -10;if(!exists(PARAM_SFO))return -11;return 0;
}
int mrw_make_head_bin(void){
    /* Always replace any packaged/stale head.bin with one generated for this app. */
    if(exists(HEAD_BIN)) sceIoRemove(HEAD_BIN);
    char tid[16]={0},cid[64]={0};int r=mrw_sfo_get_string(PARAM_SFO,"TITLE_ID",tid,sizeof(tid));
    if(r<0||!valid_tid(tid))return -20;mrw_sfo_get_string(PARAM_SFO,"CONTENT_ID",cid,sizeof(cid));
    uint8_t *h=NULL;size_t hs=0;r=read_file(HEAD_TEMPLATE,&h,&hs);if(r<0)return -21;if(hs<0x100){free(h);return -22;}
    char fallback[48]={0};snprintf(fallback,sizeof(fallback),"EP9000-%s_00-0000000000000000",tid);
    memset(h+0x30,0,48);strncpy((char*)h+0x30,cid[0]?cid:fallback,47);uint8_t mac[16];
    uint32_t n=be32(h+0xD0);if((size_t)n+16>hs){free(h);return -23;}fpkg_hmac(h,n,mac);memcpy(h+n,mac,16);
    uint32_t off=be32(h+0x08),len=be32(h+0x10),out=be32(h+0xD4);
    if(len<64||(size_t)off+len-64>hs||(size_t)out+16>hs){free(h);return -24;}
    fpkg_hmac(h+off,len-64,mac);memcpy(h+out,mac,16);
    n=be32(h+0xE8);if((size_t)n+16>hs){free(h);return -25;}fpkg_hmac(h,n,mac);memcpy(h+n,mac,16);
    sceIoMkdir(PACKAGE_TEMP "/sce_sys",0777);sceIoMkdir(PACKAGE_TEMP "/sce_sys/package",0777);
    r=write_file(HEAD_BIN,h,hs);free(h);return r;
}
static int load_paf(void){
    /*
     * Match VitaShell's proven PAF loader contract.
     *
     * The previous build passed NULL as the option/result buffer. On real
     * hardware that can make the internal loader touch invalid memory and
     * return 0x80022005 (SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS).
     */
    static uint32_t argp[] = {
        0x180000,
        (uint32_t)-1,
        (uint32_t)-1,
        1,
        (uint32_t)-1,
        (uint32_t)-1
    };

    int result = -1;
    uint32_t buf[4];
    buf[0] = sizeof(buf);
    buf[1] = (uint32_t)&result;
    buf[2] = (uint32_t)-1;
    buf[3] = (uint32_t)-1;

    return sceSysmoduleLoadModuleInternalWithArg(
        SCE_SYSMODULE_INTERNAL_PAF,
        sizeof(argp),
        argp,
        (const SceSysmoduleOpt *)buf
    );
}

static int unload_paf(void){
    uint32_t buf = 0;
    return sceSysmoduleUnloadModuleInternalWithArg(
        SCE_SYSMODULE_INTERNAL_PAF,
        0,
        NULL,
        (const SceSysmoduleOpt *)&buf
    );
}

int mrw_promote_package_temp(void){
    int r=mrw_validate_package_temp();
    if(r<0)return r;

    r=mrw_make_head_bin();
    if(r<0)return r;

    r=load_paf();
    if(r<0)return r;

    r=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if(r<0){
        unload_paf();
        return r;
    }

    r=scePromoterUtilityInit();
    if(r<0){
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
        unload_paf();
        return r;
    }

    r=scePromoterUtilityPromotePkgWithRif(PACKAGE_TEMP,1);

    {
        int er=scePromoterUtilityExit();
        if(r>=0 && er<0)r=er;
    }
    {
        int ur=sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
        if(r>=0 && ur<0)r=ur;
    }
    {
        int pr=unload_paf();
        if(r>=0 && pr<0)r=pr;
    }

    return r;
}
