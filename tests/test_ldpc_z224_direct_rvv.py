from pathlib import Path
import re
import unittest

SRC=Path(__file__).resolve().parents[1]/'vendor/openairinterface5g/openair1/PHY/CODING/nrLDPC_encoder'

def table(text,name):
    body=re.search(name+r'\[\]\s*=\s*\{([^}]+)\}',text)[1]
    return [int(x) for x in body.split(',') if x.strip()]

class Z224DirectTables(unittest.TestCase):
    def test_all_operands_map_to_the_same_base_bytes(self):
        original=(SRC/'ldpc224_byte_128.c').read_text()
        equations=[(int(m[1])*16,[int(x)*16 for x in re.findall(r'c2\[(\d+)\]',m[2])])
                   for m in re.finditer(r'd2\[(\d+)\]\s*=([^;]+);',original)]
        text=(SRC/'ldpc_bg1_z224_direct_rvv.h').read_text()
        offsets=table(text,'xsai_ldpc_z224_direct_offsets')
        self.assertEqual(offsets,[old%9856+old//9856 for _,ops in equations for old in ops])
        self.assertEqual(table(text,'xsai_ldpc_z224_direct_counts'),[len(ops) for _,ops in equations])
        self.assertEqual(table(text,'xsai_ldpc_z224_direct_targets'),[dest for dest,_ in equations])
        self.assertEqual(len(equations),46)
        self.assertEqual(len(offsets),2109)
        self.assertTrue(all(0<=offset and offset+224<=9856 for offset in offsets))
    def test_direct_dispatch_respects_both_opt_outs_and_retains_original(self):
        text=(SRC/'ldpc_encoder_optim8segmulti.c').read_text()
        self.assertIn('XSAI_USE_LDPC_BG1_Z224_RVV && !defined(XSAI_LDPC_DISABLE_RVV_BG1_Z224_DIRECT)',text)
        self.assertIn('if (xsai_ldpc_bg1_z224_can_encode(impp))',text)
        self.assertIn('encode_parity_check_part_optim(cc, dd, BG, Zc, Kb, simd_size, ncols)',text)
        self.assertIn('encode_parity_check_part_orig(cc, dd, BG, Zc, Kb, block_length)',text)
        entry=(SRC/'ldpc_bg1_z224_encoder_rvv.h').read_text()
        self.assertIn('p->BG==1 && p->Zc==224 && p->Kb==22 && p->K==4928',entry)
        self.assertIn('p->n_segments>p->first_seg',entry)
        self.assertIn('uint8_t expanded[2*22*224]',entry)

if __name__=='__main__':
    unittest.main()
