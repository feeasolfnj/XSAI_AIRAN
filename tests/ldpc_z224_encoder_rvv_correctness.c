/* Native equivalence against OAI's retained bit extraction and parity kernel. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ldpc224_byte_128.c"
#include "ldpc_bg1_z224_encoder_rvv.h"

int cpu_meas_enabled;
double cpu_freq_GHz __attribute__((aligned(32))) = 2.2;
enum { BITS=4928, INPUT_BYTES=BITS/8, PARITY=46*224, OUTPUT=3*BITS, GUARD=64 };
struct buffer { uint8_t *base,*data;size_t size,writable; };
static struct buffer allocate(size_t n) {
  size_t page=(size_t)sysconf(_SC_PAGESIZE), writable=(n+GUARD+page-1)/page*page;
  uint8_t *p=mmap(NULL,writable+2*page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  assert(p!=MAP_FAILED && !mprotect(p+page,writable,PROT_READ|PROT_WRITE));
  return (struct buffer){p,p+page+writable-n,writable+2*page,writable};
}
static uint32_t random_next(uint32_t *p) {
  uint32_t x=*p;x^=x<<13;x^=x>>17;x^=x<<5;return *p=x;
}
static void reference(uint8_t **input,unsigned first,unsigned end,uint8_t *output) {
  uint8_t cc[BITS]={0}, dd[PARITY];
  uint8_t expanded[9856*16] __attribute__((aligned(64)));
  for(unsigned i=0;i<BITS;i++)for(unsigned j=first;j<end;j++)
    cc[i]|=((input[j][i>>3]>>(7-(i&7)))&1)<<(j-first);
  for(unsigned col=0;col<22;col++) {
    memcpy(expanded+col*448,cc+col*224,224);
    memcpy(expanded+col*448+224,cc+col*224,224);
  }
  for(unsigned copy=1;copy<16;copy++)
    memcpy(expanded+9856*copy,expanded+copy,9856-copy);
  ldpc224_byte(expanded,dd);
  memcpy(output,cc+448,4480);memcpy(output+4480,dd,PARITY);
}
int main(void) {
  struct buffer src[16],out=allocate(OUTPUT);
  uint8_t *input[16],expected[OUTPUT];size_t page=(size_t)sysconf(_SC_PAGESIZE);
  for(unsigned j=0;j<16;j++){src[j]=allocate(INPUT_BYTES);input[j]=src[j].data;}
  unsigned cases=0;
  for(unsigned trial=0;trial<68;trial++) {
    uint32_t seed=20261101u+trial*2654435761u;
    for(unsigned j=0;j<16;j++) {
      assert(!mprotect(src[j].base+page,src[j].writable,PROT_READ|PROT_WRITE));
      for(unsigned i=0;i<INPUT_BYTES;i++)input[j][i]=trial==0?0:trial==1?255:trial==2?(uint8_t)(i+j):trial==3?(i&1?0x55:0xaa):(uint8_t)random_next(&seed);
      assert(!mprotect(src[j].base+page,src[j].writable,PROT_READ));
    }
    for(unsigned first=0;first<16;first+=8)for(unsigned n=1;n<=8;n++) {
      encoder_implemparams_t p={.BG=1,.Zc=224,.Kb=22,.K=BITS,.n_segments=first+n,.first_seg=first};
      assert(xsai_ldpc_bg1_z224_can_encode(&p));
      time_stats_t tin={0},tprep={0},tparity={0};
      cpu_meas_enabled=trial==0;
      if(cpu_meas_enabled){p.tinput=&tin;p.tprep=&tprep;p.tparity=&tparity;}
      reference(input,first,first+n,expected);
      memset(out.data-GUARD,0xa5,OUTPUT+GUARD);
      assert(xsai_ldpc_bg1_z224_encode_rvv(input,out.data,&p)==0);
      if(memcmp(expected,out.data,OUTPUT)) {
        fprintf(stderr,"Mismatch trial=%u first=%u n=%u\n",trial,first,n);return 1;
      }
      for(unsigned i=0;i<GUARD;i++)assert(out.data[-GUARD+(int)i]==0xa5);
      if(cpu_meas_enabled)assert(tin.trials==1 && tprep.trials==1 && tparity.trials==1);
      p.BG=2;assert(!xsai_ldpc_bg1_z224_can_encode(&p));p.BG=1;
      p.Zc=384;assert(!xsai_ldpc_bg1_z224_can_encode(&p));p.Zc=224;
      p.Kb=21;assert(!xsai_ldpc_bg1_z224_can_encode(&p));p.Kb=22;
      p.K=BITS-1;assert(!xsai_ldpc_bg1_z224_can_encode(&p));p.K=BITS;
      p.n_segments=first;assert(!xsai_ldpc_bg1_z224_can_encode(&p));
      cases++;
    }
  }
  for(unsigned j=0;j<16;j++)munmap(src[j].base,src[j].size);
  munmap(out.base,out.size);
  printf("PASS: %u complete Z224 encoder groups, 1..8 segments, first=0/8; 14784 bytes match retained OAI reference; guards, timers and eligibility checks passed\n",cases);
  return 0;
}
