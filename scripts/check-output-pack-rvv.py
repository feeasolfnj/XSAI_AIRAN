#!/usr/bin/env python3
"""Compare the RVV output packer with the retained OAI scalar fallback on Linux/RVV."""
import argparse
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile

def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--oai', type=Path, default=root/'vendor/openairinterface5g')
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--cpu', type=int)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('riscv64', 'riscv64gc'):
        parser.error('Run this native test on a 64-bit RISC-V Linux machine with RVV')
    compiler = shutil.which(args.cc)
    taskset = shutil.which('taskset')
    if not compiler or not taskset:
        parser.error('Compiler or taskset not found; pass --cc gcc-16 if appropriate')
    allowed = os.sched_getaffinity(0)
    cpu = min(allowed) if args.cpu is None else args.cpu
    if cpu not in allowed:
        parser.error(f'CPU {cpu} unavailable; allowed CPUs: {sorted(allowed)}')
    segment = args.oai.resolve()/'openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment'
    source = segment/'nrLDPC_coding_segment_encoder.c'
    header = segment/'write_task_output_rvv.h'
    test = root/'tests/output_pack_rvv_correctness.c'
    if not all(p.is_file() for p in (source, header, test)):
        parser.error('Missing optimized source/header/test; check --oai and repository version')
    text = source.read_text()
    signature = re.search(r'static void write_task_output\([^{}]*\)\s*\{', text)
    marker = '// Portable scalar fallback.'
    if signature is None or marker not in text:
        parser.error('Cannot identify the retained scalar reference in this OAI version')
    start = text.index(marker)
    end = text.index('\n#endif', start)
    reference = signature[0].replace('write_task_output', 'reference', 1)+text[start:end]+'\n}\n'
    with tempfile.TemporaryDirectory(prefix='xsai-pack-rvv-') as folder:
        tmp = Path(folder)
        (tmp/'reference.h').write_text(reference)
        exe = tmp/'test_output_pack'
        subprocess.run([compiler, '-O2', '-march=rv64gcv', '-mabi=lp64d',
                        '-I'+str(segment), '-I'+str(tmp), str(test), '-o', str(exe)], check=True)
        subprocess.run([taskset, '-c', str(cpu), str(exe)], check=True)

if __name__ == '__main__':
    main()
