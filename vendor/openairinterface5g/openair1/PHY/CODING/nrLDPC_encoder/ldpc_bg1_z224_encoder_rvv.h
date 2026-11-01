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

#ifndef XSAI_LDPC_BG1_Z224_ENCODER_RVV_H
#define XSAI_LDPC_BG1_Z224_ENCODER_RVV_H
#include "ldpc_input_z224_rvv.h"
#include "ldpc_bg1_z224_direct_rvv.h"
#include <stdint.h>
#include <string.h>
#include "openair1/PHY/CODING/nrLDPC_defs.h"
#include "time_meas.h"

/* Specialize only the already verified BG1/Z224/K4928 layout. Preserve the
 * original encoder path for every other configuration. Input expansion
 * overwrites all cc bytes and the parity kernel overwrites all dd bytes. */
static inline int xsai_ldpc_bg1_z224_can_encode(const encoder_implemparams_t *p) {
  return p->BG==1 && p->Zc==224 && p->Kb==22 && p->K==4928 && p->n_segments>p->first_seg;
}

static inline int xsai_ldpc_bg1_z224_encode_rvv(uint8_t **input,uint8_t *output,encoder_implemparams_t *p) {
  const unsigned first=p->first_seg;
  const unsigned count=p->n_segments-first>8?8:p->n_segments-first;
  const unsigned end=first+count;
  uint8_t cc[22*224] __attribute__((aligned(64)));
  uint8_t dd[46*224] __attribute__((aligned(64)));
  uint8_t expanded[2*22*224] __attribute__((aligned(64)));
  if(p->tinput)start_meas(p->tinput);
  xsai_input_expand224(input,first,end,cc);
  if(p->tinput)stop_meas(p->tinput);
  if(p->tprep)start_meas(p->tprep);
  if(p->tprep)stop_meas(p->tprep);
  if(p->tparity)start_meas(p->tparity);
  for(unsigned col=0;col<22;col++) {
    memcpy(expanded+col*448,cc+col*224,224);
    memcpy(expanded+col*448+224,cc+col*224,224);
  }
  xsai_ldpc_bg1_z224_direct_rvv(expanded,dd);
  if(p->tparity)stop_meas(p->tparity);
  /* Original BG1/Z224 rate-1/3 mother code after puncturing the first 2Z bits. */
  memcpy(output,cc+448,4480);
  memcpy(output+4480,dd,10304);
  return 0;
}

#endif
