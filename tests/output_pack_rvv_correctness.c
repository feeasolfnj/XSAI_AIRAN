#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "reference.h"
#include "write_task_output_rvv.h"

static uint32_t state = 20261030u;
static uint32_t next(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
typedef struct { uint8_t *map, *data; size_t size; } guarded;
static guarded input(size_t size) {
  const size_t page=(size_t)sysconf(_SC_PAGESIZE);
  const size_t rounded=((size+page-1)/page)*page;
  uint8_t *p=mmap(0,rounded+page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(p==MAP_FAILED || mprotect(p+rounded,page,PROT_NONE))exit(2);
  uint8_t *data=p+rounded-size;
  for(size_t i=0;i<size;i++)data[i]=(uint8_t)next();
  return (guarded){p,data,rounded+page};
}
int main(void) {
  const uint32_t lengths[]={0,1,7,8,9,15,16,31,32,33,63,64,65,127,128,129,255,256,257,15000,15004};
  unsigned cases=0;
  for(unsigned z=0;z<sizeof(lengths)/sizeof(lengths[0]);z++) {
    uint32_t E=lengths[z], E2=E+4;
    guarded f=input(E),f2=input(E2);
    for(unsigned n=1;n<=8;n++)for(unsigned offset=0;offset<64;offset++)
      for(unsigned split=0;split<3;split++)for(unsigned init=0;init<3;init++) {
        unsigned first=split==0?0:split==1?n/2:n;
        size_t size=128+(offset+n*(size_t)E2+7)/8;
        uint8_t *a=malloc(size),*b=malloc(size);if(!a||!b)return 2;
        for(size_t i=0;i<size;i++)a[i]=init==0?0:init==1?255:(uint8_t)next();
        memcpy(b,a,size);
        reference(f.data,E,f2.data,E2,true,first,n,a+64,offset);
        xsai_write_task_output_rvv(f.data,E,f2.data,E2,first,n,b+64,offset);
        if(memcmp(a,b,size)) {
          fprintf(stderr,"FAIL E=%u E2=%u n=%u offset=%u first=%u init=%u\n",E,E2,n,offset,first,init);
          return 1;
        }
        free(a);free(b);cases++;
      }
    munmap(f.map,f.size);munmap(f2.map,f2.size);
  }
  printf("PASS cases=%u original_scalar_byte_for_byte=passed output_guards=passed input_guard_pages=passed initial_output=zero,ones,random\n",cases);
  return 0;
}
