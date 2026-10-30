from pathlib import Path
import re, unittest

SRC = Path(__file__).resolve().parents[1] / 'vendor/openairinterface5g/openair1/PHY/CODING/nrLDPC_encoder'

class Z224Tables(unittest.TestCase):
    def test_exact_operands_and_destinations(self):
        ref = (SRC / 'ldpc224_byte_128.c').read_text()
        equations = [(int(m[1])*16, [int(n)*16 for n in re.findall(r'c2\[(\d+)\]', m[2])])
                     for m in re.finditer(r'd2\[(\d+)\]\s*=([^;]+);', ref)]
        self.assertEqual(len(equations), 46)
        header = (SRC / 'ldpc_bg1_z224_rvv.h').read_text()
        def table(name):
            body = re.search('xsai_ldpc_z224_' + name + r'\[\]\s*=\s*\{([^}]+)\}', header)[1]
            return [int(x) for x in body.split(',') if x.strip()]
        self.assertEqual(table('offsets'), [x for _, terms in equations for x in terms])
        self.assertEqual(table('counts'), [len(terms) for _, terms in equations])
        self.assertEqual(table('targets'), [dest for dest, _ in equations])
    def test_scoped_dispatch_and_existing_fallbacks(self):
        text = (SRC / 'ldpc_encode_parity_check.c').read_text()
        self.assertIn('defined(__riscv_vector) && !defined(__AVX2__) && !defined(__AVX512F__)', text)
        self.assertIn('XSAI_LDPC_DISABLE_RVV_BG1_Z224', text)
        bg1, bg2 = text.split('} else if (BG == 2)', 1)
        branch = bg1.split('case 224:', 1)[1].split('break;', 1)[0]
        self.assertIn('xsai_ldpc_bg1_z224_rvv(c, d)', branch)
        self.assertIn('ldpc224_byte(c, d)', branch)
        self.assertIn('xsai_ldpc_bg1_z384_rvv(c, d)', bg1)
        self.assertNotIn('xsai_ldpc_bg1_z224_rvv(c, d)', bg2)
        self.assertIn('ldpc_BG2_Zc224_byte(c, d)', bg2)

if __name__ == '__main__':
    unittest.main()
