
#include "../include/sha1.h"
#include <string.h>

static uint32_t rol32(uint32_t v, unsigned n){ return (v<<n)|(v>>(32-n)); }
static uint32_t be32(const uint8_t *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static void transform(MrwSha1 *c,const uint8_t b[64]){
    uint32_t w[80],a,bv,cc,d,e,f,k,t; int i;
    for(i=0;i<16;i++) w[i]=be32(b+i*4);
    for(i=16;i<80;i++) w[i]=rol32(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=c->state[0]; bv=c->state[1]; cc=c->state[2]; d=c->state[3]; e=c->state[4];
    for(i=0;i<80;i++){
        if(i<20){f=(bv&cc)|((~bv)&d);k=0x5A827999;}
        else if(i<40){f=bv^cc^d;k=0x6ED9EBA1;}
        else if(i<60){f=(bv&cc)|(bv&d)|(cc&d);k=0x8F1BBCDC;}
        else{f=bv^cc^d;k=0xCA62C1D6;}
        t=rol32(a,5)+f+e+k+w[i]; e=d; d=cc; cc=rol32(bv,30); bv=a; a=t;
    }
    c->state[0]+=a;c->state[1]+=bv;c->state[2]+=cc;c->state[3]+=d;c->state[4]+=e;
}
void mrw_sha1_init(MrwSha1 *c){
    c->state[0]=0x67452301;c->state[1]=0xEFCDAB89;c->state[2]=0x98BADCFE;
    c->state[3]=0x10325476;c->state[4]=0xC3D2E1F0;c->count=0;memset(c->buffer,0,64);
}
void mrw_sha1_update(MrwSha1 *c,const void *p_,size_t n){
    const uint8_t *p=p_; size_t used=(size_t)(c->count&63); c->count+=n;
    if(used){ size_t take=(n<64-used)?n:64-used; memcpy(c->buffer+used,p,take);
        used+=take;p+=take;n-=take;if(used==64)transform(c,c->buffer);}
    while(n>=64){transform(c,p);p+=64;n-=64;}
    if(n)memcpy(c->buffer,p,n);
}
void mrw_sha1_final(MrwSha1 *c,uint8_t out[20]){
    uint64_t bits=c->count*8; uint8_t pad[72]={0}; size_t used=(size_t)(c->count&63);
    size_t pn=used<56?56-used:120-used; pad[0]=0x80; mrw_sha1_update(c,pad,pn);
    uint8_t lb[8]; for(int i=0;i<8;i++)lb[7-i]=(uint8_t)(bits>>(i*8));
    mrw_sha1_update(c,lb,8);
    for(int i=0;i<5;i++){out[i*4]=c->state[i]>>24;out[i*4+1]=c->state[i]>>16;
        out[i*4+2]=c->state[i]>>8;out[i*4+3]=c->state[i];}
}
