#!/usr/bin/env python3
"""Run BG1/Z224 native RVV input, direct-kernel and complete-group equivalence."""
import argparse
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import tempfile

def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--oai',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--cc',default='gcc-16')
    parser.add_argument('--cpu',type=int)
    args=parser.parse_args()
    if platform.system()!='Linux' or platform.machine() not in ('riscv64','riscv64gc'):
        parser.error('Run native checks on RVV riscv64 Linux; table checks can run on other hosts')
    allowed=os.sched_getaffinity(0);cpu=min(allowed) if args.cpu is None else args.cpu
    if cpu not in allowed:parser.error(f'CPU {cpu} unavailable; allowed: {sorted(allowed)}')
    if not shutil.which(args.cc) or not shutil.which('taskset'):
        parser.error('Compiler or taskset missing; use --cc to select a compiler with RVV intrinsics')
    oai=args.oai.resolve();build=args.build.resolve()
    recipe=build/'openair1/PHY/CODING/CMakeFiles/ldpc.dir/flags.make'
    if not recipe.is_file():parser.error('Need an existing compatible OAI Makefile build with ldpc flags.make')
    fields=dict(line.split(' = ',1) for line in recipe.read_text().splitlines() if ' = ' in line)
    flags=shlex.split(' '.join(fields[k] for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']))
    enc=oai/'openair1/PHY/CODING/nrLDPC_encoder'
    names=['ldpc224_byte_128.c','ldpc_input_z224_rvv.h','ldpc_bg1_z224_direct_rvv.h','ldpc_bg1_z224_encoder_rvv.h']
    for name in names:
        if not (enc/name).is_file():parser.error('Missing '+str(enc/name))
    tests=['ldpc_z224_input_rvv_correctness','ldpc_z224_direct_rvv_correctness','ldpc_z224_encoder_rvv_correctness']
    with tempfile.TemporaryDirectory(prefix='xsai-z224-direct-') as folder:
        for name in tests:
            exe=Path(folder)/name
            cmd=[args.cc,'-I'+str(oai),'-I'+str(enc),*flags,'-UNDEBUG','-O2',str(root/'tests'/(name+'.c')),'-o',str(exe)]
            print('Compile:',shlex.join(cmd),flush=True)
            subprocess.run(cmd,cwd=build,check=True)
            subprocess.run(['taskset','-c',str(cpu),str(exe)],check=True)

if __name__=='__main__':
    main()
