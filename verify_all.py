import os
import subprocess
import re
import sys

sim_path = os.path.abspath("riscv_sim_base/rvsim.exe")
base_dir = os.path.abspath("riscv_sim_base")

tests = [
    "tests/micro.s",
    "tests/sample1_sum_loop.s",
    "tests/sample2_mem_and_branches.s"
]

def run_sim(test_file, mode_flag):
    cmd = [sim_path, test_file]
    if mode_flag:
        cmd.append(mode_flag)
    p = subprocess.run(cmd, cwd=base_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return p.stdout

def extract_regs(output):
    regs = {}
    for line in output.splitlines():
        # Match lines like x0 /zero=0x00000000 ...
        for m in re.finditer(r'(x\d+)\s*/[a-z0-9]+\s*=\s*(0x[0-9a-fA-F]+)', line):
            regs[m.group(1)] = m.group(2).lower()
    return regs

def extract_pipeline_stats(output):
    m = re.search(r'pipeline:\s+mode=(\w+)\s+cycles=(\d+)\s+retired=(\d+)\s+stalls=(\d+)\s+flushes=(\d+)\s+CPI=([\d.]+)\s+fwd_ex=(\d+)\s+fwd_mem=(\d+)\s+load_use=(\d+)', output)
    if m:
        return {
            "mode": m.group(1),
            "cycles": int(m.group(2)),
            "retired": int(m.group(3)),
            "stalls": int(m.group(4)),
            "flushes": int(m.group(5)),
            "cpi": float(m.group(6)),
            "fwd_ex": int(m.group(7)),
            "fwd_mem": int(m.group(8)),
            "load_use": int(m.group(9))
        }
    return None

def extract_batch_cycles(output):
    m = re.search(r'cycles\s*=\s*(\d+)', output)
    return int(m.group(1)) if m else None

print(f"{'Program':<26} | {'Mode':<16} | {'Cycles':<6} | {'Retired':<7} | {'Stalls':<6} | {'Flushes':<7} | {'CPI':<6} | {'Reg Match':<10}")
print("-" * 105)

all_passed = True

for t in tests:
    out_batch = run_sim(t, "--batch")
    out_pipe  = run_sim(t, "--pipeline")
    out_fwd   = run_sim(t, "--pipeline-forward")

    regs_batch = extract_regs(out_batch)
    regs_pipe  = extract_regs(out_pipe)
    regs_fwd   = extract_regs(out_fwd)

    batch_cycles = extract_batch_cycles(out_batch)
    stats_pipe = extract_pipeline_stats(out_pipe)
    stats_fwd  = extract_pipeline_stats(out_fwd)

    match_pipe = (regs_batch == regs_pipe)
    match_fwd  = (regs_batch == regs_fwd)

    print(f"{t:<26} | {'--batch (oracle)':<16} | {batch_cycles:<6} | {batch_cycles:<7} | {'-':<6} | {'-':<7} | {'1.000':<6} | {'MATCH':<10}")
    print(f"{t:<26} | {'--pipeline':<16} | {stats_pipe['cycles']:<6} | {stats_pipe['retired']:<7} | {stats_pipe['stalls']:<6} | {stats_pipe['flushes']:<7} | {stats_pipe['cpi']:<6.3f} | {('PASS' if match_pipe else 'FAIL'):<10}")
    print(f"{t:<26} | {'--pipeline-fwd':<16} | {stats_fwd['cycles']:<6} | {stats_fwd['retired']:<7} | {stats_fwd['stalls']:<6} | {stats_fwd['flushes']:<7} | {stats_fwd['cpi']:<6.3f} | {('PASS' if match_fwd else 'FAIL'):<10}")
    print("-" * 105)

    # Check invariants
    # 1. retired == batch_cycles
    if stats_pipe['retired'] != batch_cycles or stats_fwd['retired'] != batch_cycles:
        print(f"ERROR: Retired instructions mismatch in {t}!")
        all_passed = False

    # 2. cycles = retired + stalls + flushes + 4
    if stats_pipe['cycles'] != (stats_pipe['retired'] + stats_pipe['stalls'] + stats_pipe['flushes'] + 4):
        print(f"ERROR: Lab 2 Identity failed in {t}!")
        all_passed = False

    if stats_fwd['cycles'] != (stats_fwd['retired'] + stats_fwd['stalls'] + stats_fwd['flushes'] + 4):
        print(f"ERROR: Lab 3 Identity failed in {t}!")
        all_passed = False

    if not match_pipe or not match_fwd:
        all_passed = False

if all_passed:
    print("\nALL INVARIANTS AND REGISTERS MATCH 100% PERFECTLY!")
else:
    print("\nSOME TESTS FAILED")
