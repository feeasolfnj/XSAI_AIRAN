#!/usr/bin/env python3
"""Compile and run the native BG1/Zc224 RVV kernel comparison using a local OAI build."""
import argparse,json,pathlib,platform,shlex,shutil,subprocess,tempfile
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--oai',type=pathlib.Path,required=True)
    p.add_argument('--build',type=pathlib.Path,required=True)
    p.add_argument('--cc',default='gcc-16');p.add_argument('--cpu',type=int,default=0)
    a=p.parse_args()
    if platform.machine()!='riscv64':p.error('Native execution requires riscv64 with RVV; run table tests on other hosts')
    root=pathlib.Path(__file__).resolve().parents[1];oai=a.oai.resolve();build=a.build.resolve()
    recipe=build/'openair1/PHY/CODING/CMakeFiles/ldpc.dir/flags.make'
    if not recipe.is_file():p.error('Need an OAI Makefile build with ldpc flags.make; configure/build OAI first')
    fields=dict(line.split(' = ',1) for line in recipe.read_text().splitlines() if ' = ' in line)
    flags=shlex.split(' '.join(fields[k] for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']))
    enc=oai/'openair1/PHY/CODING/nrLDPC_encoder'
    for name in ['ldpc224_byte_128.c','ldpc_bg1_z224_rvv.h']:
        if not (enc/name).is_file():p.error('Missing '+str(enc/name))
    if not shutil.which(a.cc):p.error('Compiler not found: '+a.cc)
    with tempfile.TemporaryDirectory(prefix='xsai-ldpc-rvv-') as d:
        exe=pathlib.Path(d)/'check'
        cmd=[a.cc,*flags,'-O2','-I'+str(enc),str(root/'tests/ldpc_z224_rvv_correctness.c'),'-o',str(exe)]
        print('Compile:',shlex.join(cmd),flush=True)
        subprocess.run(cmd,cwd=build,check=True)
        subprocess.run(['taskset','-c',str(a.cpu),str(exe)],check=True)
if __name__=='__main__':main()
