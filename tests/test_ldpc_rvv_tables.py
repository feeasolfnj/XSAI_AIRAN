"""Prevent drift between RVV operand tables and the vendored OAI reference."""
import pathlib,re,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
ENC=ROOT/'vendor/openairinterface5g/openair1/PHY/CODING/nrLDPC_encoder'
class LdpcRvvTables(unittest.TestCase):
    def test_all_xor_operands_and_destinations_match_reference(self):
        reference=(ENC/'ldpc384_byte_128.c').read_text()
        expected=[]
        for match in re.finditer(r'd2\[(\d+)\]\s*=([^;]+);',reference):
            expected.append((int(match[1])*16,[int(n)*16 for n in re.findall(r'c2\[(\d+)\]',match[2])]))
        self.assertEqual(len(expected),46)
        header=(ENC/'ldpc_bg1_z384_rvv.h').read_text()
        def table(name):
            body=re.search(name+r'\[[^]]*\]\s*=\s*\{([^}]+)\}',header)[1]
            return [int(n) for n in body.split(',')]
        counts=table('xsai_ldpc_counts');targets=table('xsai_ldpc_targets');offsets=table('xsai_ldpc_offsets')
        self.assertEqual(counts,[len(terms) for _,terms in expected])
        self.assertEqual(targets,[target for target,_ in expected])
        self.assertEqual(offsets,[offset for _,terms in expected for offset in terms])
    def test_target_guard_and_original_fallback_remain(self):
        text=(ENC/'ldpc_encode_parity_check.c').read_text()
        self.assertIn('defined(__riscv_vector) && !defined(__AVX2__) && !defined(__AVX512F__)',text)
        self.assertIn('XSAI_LDPC_DISABLE_RVV_BG1_Z384',text)
        branch=text.split('case 384:',1)[1].split('break;',1)[0]
        self.assertIn('xsai_ldpc_bg1_z384_rvv(c, d)',branch)
        self.assertIn('ldpc384_byte(c, d)',branch)
if __name__=='__main__':unittest.main()
