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

#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ldpc_input_z224_rvv.h"
enum {BITS=4928,BYTES=BITS/8,GUARD=64};
struct buffer {uint8_t *base,*data;size_t size,writable;};
static struct buffer allocate(size_t n) {
  size_t page=sysconf(_SC_PAGESIZE), writable=(n+GUARD+page-1)/page*page;
  uint8_t *p=mmap(NULL,writable+page*2,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  assert(p!=MAP_FAILED && !mprotect(p+page,writable,PROT_READ|PROT_WRITE));
  return (struct buffer){p,p+page+writable-n,writable+page*2,writable};
}
static uint32_t next(uint32_t *p){uint32_t x=*p;x^=x<<13;x^=x>>17;x^=x<<5;return *p=x;}
int main(void) {
  struct buffer src[16],dst=allocate(BITS);uint8_t *inputs[16],expected[BITS];
  size_t page=sysconf(_SC_PAGESIZE);
  for(unsigned i=0;i<16;i++){src[i]=allocate(BYTES);inputs[i]=src[i].data;}
  unsigned cases=0;
  for(unsigned trial=0;trial<68;trial++) {
    uint32_t seed=20261031u+trial*2654435761u;
    for(unsigned j=0;j<16;j++) {
      assert(!mprotect(src[j].base+page,src[j].writable,PROT_READ|PROT_WRITE));
      for(unsigned i=0;i<BYTES;i++) inputs[j][i]=trial==0?0:trial==1?255:trial==2?(uint8_t)(i+j):trial==3?(i&1?0xaa:0x55):(uint8_t)next(&seed);
      assert(!mprotect(src[j].base+page,src[j].writable,PROT_READ));
    }
    for(unsigned first=0;first<16;first+=8)for(unsigned n=1;n<=8;n++) {
      memset(expected,0,sizeof(expected));
      for(unsigned i=0;i<BITS;i++)for(unsigned j=first;j<first+n;j++)
        expected[i]|=((inputs[j][i>>3]>>(7-(i&7)))&1)<<(j-first);
      memset(dst.data-GUARD,0xa5,BITS+GUARD);
      xsai_input_expand224(inputs,first,first+n,dst.data);
      if(memcmp(expected,dst.data,BITS)){fprintf(stderr,"Mismatch trial=%u first=%u n=%u\n",trial,first,n);return 1;}
      for(unsigned i=0;i<GUARD;i++)assert(dst.data[-GUARD+(int)i]==0xa5);
      cases++;
    }
  }
  for(unsigned j=0;j<16;j++)munmap(src[j].base,src[j].size);
  munmap(dst.base,dst.size);
  printf("PASS: %u input expansion cases, original bit-plane mapping, read-only input and end guards\n",cases);
  return 0;
}
