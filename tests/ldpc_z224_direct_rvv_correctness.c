/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/* Native equivalence test against the existing OAI 128-bit-layout kernel. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ldpc224_byte_128.c"
#include "ldpc_bg1_z224_direct_rvv.h"
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
  struct buffer direct=guarded(9856);
  for(unsigned trial=0;trial<68;trial++) {
    uint32_t s=20261030u+trial*2654435761u; if(!s)s=1;
    for(size_t i=0;i<INPUT;i++) c.data[i]=trial==0?0:trial==1?255:trial==2?(uint8_t)i:trial==3?(i&1?0x55:0xaa):(uint8_t)next(&s);
    for(unsigned copy=1;copy<16;copy++)memcpy(c.data+9856*copy,c.data+copy,9856-copy);
    memcpy(direct.data,c.data,9856);
    memset(a.data-GUARD,0xa5,OUTPUT+GUARD); memset(b.data-GUARD,0xa5,OUTPUT+GUARD);
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    if(mprotect(c.mapping+page,c.writable,PROT_READ))return 2;
    if(mprotect(direct.mapping+page,direct.writable,PROT_READ))return 2;
    ldpc224_byte(c.data,a.data); xsai_ldpc_bg1_z224_direct_rvv(direct.data,b.data);
    if(mprotect(direct.mapping+page,direct.writable,PROT_READ|PROT_WRITE))return 2;
    if(memcmp(a.data,b.data,OUTPUT)) {
      for(size_t i=0;i<OUTPUT;i++)if(a.data[i]!=b.data[i]) { fprintf(stderr,"Mismatch trial=%u byte=%zu ref=%u got=%u\n",trial,i,a.data[i],b.data[i]); break; }
      return 1;
    }
    for(size_t i=0;i<GUARD;i++)if(a.data[-GUARD+(int)i]!=0xa5 || b.data[-GUARD+(int)i]!=0xa5)return 1;
    if(mprotect(c.mapping+page,c.writable,PROT_READ|PROT_WRITE))return 2;
  }
  munmap(direct.mapping,direct.size);
  munmap(c.mapping,c.size); munmap(a.mapping,a.size); munmap(b.mapping,b.size);
  puts("PASS: Zc224, 68 base matrices: direct addressing equals all 16 shifted banks (4 structured + 64 random), 10304 bytes each match original; input read-only, output prefix canaries and end guard pages intact");
  return 0;
}
