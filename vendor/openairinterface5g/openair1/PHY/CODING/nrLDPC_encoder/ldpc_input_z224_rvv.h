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

#ifndef XSAI_LDPC_INPUT_Z224_RVV_H
#define XSAI_LDPC_INPUT_Z224_RVV_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <riscv_vector.h>

/* Preserve OAI's MSB-first packed input and one bit plane per segment.
 * The caller limits this specialization to BG1/Zc224, K=4928 bits. */
static inline void xsai_input_expand224(uint8_t **input,unsigned first,unsigned end,uint8_t *cc) {
  assert(end>first && end-first<=8);
  const size_t n=__riscv_vsetvl_e8m2(64);
  vuint8m2_t ids=__riscv_vid_v_u8m2(n);
  vuint8m2_t index=__riscv_vsrl_vx_u8m2(ids,3,n);
  vuint8m2_t shifts=__riscv_vrsub_vx_u8m2(__riscv_vand_vx_u8m2(ids,7,n),7,n);
  for(size_t i=0;i<4928;i+=n) {
    vuint8m2_t accum=__riscv_vmv_v_x_u8m2(0,n);
    for(unsigned j=first;j<end;j++) {
      vuint8m2_t packed=__riscv_vle8_v_u8m2(input[j]+(i>>3),n/8);
      vuint8m2_t expanded=__riscv_vrgather_vv_u8m2(packed,index,n);
      vuint8m2_t bits=__riscv_vand_vx_u8m2(__riscv_vsrl_vv_u8m2(expanded,shifts,n),1,n);
      accum=__riscv_vor_vv_u8m2(accum,__riscv_vsll_vx_u8m2(bits,j-first,n),n);
    }
    __riscv_vse8_v_u8m2(cc+i,accum,n);
  }
}

#endif
