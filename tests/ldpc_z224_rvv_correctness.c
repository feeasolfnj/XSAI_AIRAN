/* Native equivalence test against the existing OAI 128-bit-layout kernel. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ldpc224_byte_128.c"
#include "ldpc_bg1_z224_rvv.h"
#if !defined(__riscv_vector) || defined(__AVX2__) || defined(__AVX512F__)
#error Requires native RVV and the original non-AVX 128-bit layout
#endif
enum { INPUT=2*22*224*16, OUTPUT=46*224, GUARD=64 };
static uint32_t next(uint32_t *s) {
  uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; return *s=x;
}
struct buffer { uint8_t *mapping, *data; size_t size, writable; };
static struct buffer guarded(size_t length) {
  size_t page=(size_t)sysconf(_SC_PAGESIZE);
  size_t writable=(length+GUARD+page-1)/page*page;
  uint8_t *p=mmap(NULL,writable+2*page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(p==MAP_FAILED || mprotect(p+page,writable,PROT_READ|PROT_WRITE)) { perror("mmap"); exit(2); }
  return (struct buffer){p,p+page+writable-length,writable+2*page,writable};
}
int main(void) {
  struct buffer c=guarded(INPUT), a=guarded(OUTPUT), b=guarded(OUTPUT);
  for(unsigned trial=0;trial<68;trial++) {
    uint32_t s=20261030u+trial*2654435761u; if(!s)s=1;
    for(size_t i=0;i<INPUT;i++) c.data[i]=trial==0?0:trial==1?255:trial==2?(uint8_t)i:trial==3?(i&1?0x55:0xaa):(uint8_t)next(&s);
    memset(a.data-GUARD,0xa5,OUTPUT+GUARD); memset(b.data-GUARD,0xa5,OUTPUT+GUARD);
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    if(mprotect(c.mapping+page,c.writable,PROT_READ))return 2;
    ldpc224_byte(c.data,a.data); xsai_ldpc_bg1_z224_rvv(c.data,b.data);
    if(memcmp(a.data,b.data,OUTPUT)) {
      for(size_t i=0;i<OUTPUT;i++)if(a.data[i]!=b.data[i]) { fprintf(stderr,"Mismatch trial=%u byte=%zu ref=%u got=%u\n",trial,i,a.data[i],b.data[i]); break; }
      return 1;
    }
    for(size_t i=0;i<GUARD;i++)if(a.data[-GUARD+(int)i]!=0xa5 || b.data[-GUARD+(int)i]!=0xa5)return 1;
    if(mprotect(c.mapping+page,c.writable,PROT_READ|PROT_WRITE))return 2;
  }
  munmap(c.mapping,c.size); munmap(a.mapping,a.size); munmap(b.mapping,b.size);
  puts("PASS: Zc224, 68 expanded matrices (4 structured + 64 random), 10304 bytes each match original; input read-only, output prefix canaries and end guard pages intact");
  return 0;
}
