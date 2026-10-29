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

/* Independent execution comparison against the original selected OAI kernel. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ldpc384_byte_128.c"
#include "ldpc_bg1_z384_rvv.h"
#if !defined(__riscv_vector) || defined(__AVX2__)
#error This native test requires RISC-V RVV and the original 128-bit layout
#endif
static uint32_t next(uint32_t *s) { uint32_t x=*s; x^=x<<13;x^=x>>17;x^=x<<5;return *s=x; }
int main(void) {
  enum { INPUT=2*22*384*32, OUTPUT=46*384, GUARD=64 };
  uint8_t *c=aligned_alloc(64,INPUT),*reference=aligned_alloc(64,OUTPUT+GUARD),*candidate=aligned_alloc(64,OUTPUT+GUARD);
  if(!c||!reference||!candidate)return 2;
  for(unsigned trial=0;trial<16;trial++) {
    uint32_t state=20261022u+trial*2654435761u;if(!state)state=1;
    for(size_t i=0;i<INPUT;i++)c[i]=(uint8_t)next(&state);
    memset(reference,0xa5,OUTPUT+GUARD);memset(candidate,0xa5,OUTPUT+GUARD);
    ldpc384_byte(c,reference);xsai_ldpc_bg1_z384_rvv(c,candidate);
    if(memcmp(reference,candidate,OUTPUT)) { fprintf(stderr,"Mismatch trial=%u\n",trial);return 1; }
    for(size_t i=OUTPUT;i<OUTPUT+GUARD;i++)if(reference[i]!=0xa5||candidate[i]!=0xa5)return 1;
  }
  free(c);free(reference);free(candidate);
  puts("PASS: 16 independent random expanded matrices, 17664 output bytes each, original kernel and RVV match; output guards intact");
  return 0;
}
