/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/* Derived from the OAI write_task_output RISC-V branch.
 * Retains the surrounding OAI license and NOTICE obligations.
 * Extract one bit per input byte, pack into a mask, OR into output.
 */
#ifndef XSAI_WRITE_TASK_OUTPUT_RVV_H
#define XSAI_WRITE_TASK_OUTPUT_RVV_H
#include <stdint.h>
#include <stddef.h>
#include <riscv_vector.h>

static inline void xsai_write_task_output_rvv(
    const uint8_t *f, uint32_t E, const uint8_t *f2, uint32_t E2,
    uint32_t E2_first_segment, uint32_t nb_segments,
    uint8_t *output, uint32_t Eoffset)
{
  uint32_t segment_offset = Eoffset;
  for (uint32_t j = 0; j < nb_segments; ++j) {
    const uint8_t *src = j < E2_first_segment ? f : f2;
    const uint32_t length = j < E2_first_segment ? E : E2;
    const uint8_t select = (uint8_t)(1u << j);
    uint32_t i = 0;

    /* Preserve every neighboring bit in a shared first output byte. */
    for (; i < length && ((segment_offset + i) & 7u); ++i) {
      const uint32_t bit = segment_offset + i;
      output[bit >> 3] |= ((src[i] >> j) & 1u) << (bit & 7u);
    }

    /* Store whole output bytes only: mask tail bits are not defined. */
    while (length - i >= 8) {
      const size_t remaining = (length - i) & ~7u;
      const size_t request = remaining < 64 ? remaining : 64;
      size_t vl = __riscv_vsetvl_e8m2(request);
      vl &= ~(size_t)7;
      if (!vl) break;
      const vuint8m2_t data = __riscv_vle8_v_u8m2(src + i, vl);
      const vuint8m2_t selected = __riscv_vand_vx_u8m2(data, select, vl);
      vbool4_t bits = __riscv_vmsne_vx_u8m2_b4(selected, 0, vl);
      uint8_t *dst = output + ((segment_offset + i) >> 3);
      const vbool4_t existing = __riscv_vlm_v_b4(dst, vl);
      bits = __riscv_vmor_mm_b4(bits, existing, vl);
      __riscv_vsm_v_b4(dst, bits, vl);
      i += (uint32_t)vl;
    }

    /* Preserve unused bits of the last output byte; no input overread. */
    for (; i < length; ++i) {
      const uint32_t bit = segment_offset + i;
      output[bit >> 3] |= ((src[i] >> j) & 1u) << (bit & 7u);
    }
    segment_offset += length;
  }
}
#endif
