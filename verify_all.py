import os
import subprocess
import re
import sys

sim_path = os.path.abspath("riscv_sim_base/rvsim.exe")
base_dir = os.path.abspath("riscv_sim_base")

def run_sim(test_file, args):
    cmd = [sim_path, test_file] + args
    p = subprocess.run(cmd, cwd=base_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return p.stdout

def extract_regs(output):
    regs = {}
    for line in output.splitlines():
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

def extract_bp_stats(output):
    m = re.search(r'cycles\s*=\s*(\d+).*?retired\s*=\s*(\d+).*?stalls\s*=\s*(\d+).*?branches\s*=\s*(\d+).*?mispredicts\s*=\s*(\d+).*?jal\s*=\s*(\d+).*?jalr\s*=\s*(\d+).*?redirects\s*=\s*(\d+).*?squashed\s*=\s*(\d+)', output, re.DOTALL)
    if m:
        return {
            "cycles": int(m.group(1)),
            "retired": int(m.group(2)),
            "stalls": int(m.group(3)),
            "branches": int(m.group(4)),
            "mispredicts": int(m.group(5)),
            "jal": int(m.group(6)),
            "jalr": int(m.group(7)),
            "redirects": int(m.group(8)),
            "squashed": int(m.group(9))
        }
    return None

def extract_batch_cycles(output):
    m = re.search(r'cycles\s*=\s*(\d+)', output)
    return int(m.group(1)) if m else None

all_passed = True

print("=" * 110)
print("PART 1: LAB 1, LAB 2, LAB 3 VERIFICATION (Single-Cycle vs Stalling vs Forwarding Pipeline)")
print("=" * 110)
print(f"{'Program':<30} | {'Mode':<18} | {'Cycles':<6} | {'Retired':<7} | {'Stalls':<6} | {'Flushes':<7} | {'CPI':<6} | {'Reg Match':<10}")
print("-" * 110)

lab123_tests = [
    "tests/micro.s",
    "tests/sample1_sum_loop.s",
    "tests/sample2_mem_and_branches.s",
    "tests/load_use_test.s"
]

for t in lab123_tests:
    out_batch = run_sim(t, ["--batch"])
    out_pipe  = run_sim(t, ["--pipeline"])
    out_fwd   = run_sim(t, ["--pipeline-forward"])

    regs_batch = extract_regs(out_batch)
    regs_pipe  = extract_regs(out_pipe)
    regs_fwd   = extract_regs(out_fwd)

    batch_cycles = extract_batch_cycles(out_batch)
    stats_pipe = extract_pipeline_stats(out_pipe)
    stats_fwd  = extract_pipeline_stats(out_fwd)

    match_pipe = (regs_batch == regs_pipe)
    match_fwd  = (regs_batch == regs_fwd)

    print(f"{t:<30} | {'--batch (oracle)':<18} | {batch_cycles:<6} | {batch_cycles:<7} | {'-':<6} | {'-':<7} | {'1.000':<6} | {'MATCH':<10}")
    print(f"{t:<30} | {'--pipeline':<18} | {stats_pipe['cycles']:<6} | {stats_pipe['retired']:<7} | {stats_pipe['stalls']:<6} | {stats_pipe['flushes']:<7} | {stats_pipe['cpi']:<6.3f} | {('PASS' if match_pipe else 'FAIL'):<10}")
    print(f"{t:<30} | {'--pipeline-fwd':<18} | {stats_fwd['cycles']:<6} | {stats_fwd['retired']:<7} | {stats_fwd['stalls']:<6} | {stats_fwd['flushes']:<7} | {stats_fwd['cpi']:<6.3f} | {('PASS' if match_fwd else 'FAIL'):<10}")
    print("-" * 110)

    # Invariants
    if stats_pipe['retired'] != batch_cycles or stats_fwd['retired'] != batch_cycles:
        print(f"ERROR: Retired instructions mismatch in {t}!")
        all_passed = False
    # Lab 2 & 3 identity: cycles = retired + stalls + 2*flushes + 4
    if stats_pipe['cycles'] != (stats_pipe['retired'] + stats_pipe['stalls'] + 2 * stats_pipe['flushes'] + 4):
        print(f"ERROR: Lab 2 Identity failed in {t}!")
        all_passed = False
    if stats_fwd['cycles'] != (stats_fwd['retired'] + stats_fwd['stalls'] + 2 * stats_fwd['flushes'] + 4):
        print(f"ERROR: Lab 3 Identity failed in {t}!")
        all_passed = False
    if not match_pipe or not match_fwd:
        all_passed = False

print("\n" + "=" * 110)
print("PART 2: LAB 4 BRANCH PREDICTION BENCHMARK & ORACLE VERIFICATION (W9 Section 11)")
print("=" * 110)

bp_tests = [
    "tests/t1_counted_loop.s",
    "tests/t2_nested_loops.s",
    "tests/t3_alternating.s",
    "tests/t4_calls.s"
]

golden_bp = {
    "tests/t1_counted_loop.s": {
        "retired": 37,
        "branches": 10,
        "jal": 0, "jalr": 0,
        "cycles": {"not-taken": 59, "btfn": 43, "1bit": 45, "2bit": 45},
        "mispredicts": {"not-taken": 9, "btfn": 1, "1bit": 2, "2bit": 2}
    },
    "tests/t2_nested_loops.s": {
        "retired": 52,
        "branches": 16,
        "jal": 0, "jalr": 0,
        "cycles": {"not-taken": 78, "btfn": 66, "1bit": 76, "2bit": 70},
        "mispredicts": {"not-taken": 11, "btfn": 5, "1bit": 10, "2bit": 7}
    },
    "tests/t3_alternating.s": {
        "retired": 50,
        "branches": 16,
        "jal": 4, "jalr": 0,
        "cycles": {"not-taken": 76, "btfn": 64, "1bit": 74, "2bit": 74},
        "mispredicts": {"not-taken": 11, "btfn": 5, "1bit": 10, "2bit": 10}
    },
    "tests/t4_calls.s": {
        "retired": 219,
        "branches": 20,
        "jal": 33, "jalr": 18,
        "cycles": {"not-taken": 278, "btfn": 278, "1bit": 280, "2bit": 278},
        "mispredicts": {"not-taken": 2, "btfn": 2, "1bit": 3, "2bit": 2}
    }
}

print(f"{'Program':<25} | {'Predictor':<12} | {'Cycles':<6} | {'Retired':<7} | {'Branches':<8} | {'Misses':<6} | {'JAL':<4} | {'JALR':<4} | {'Reg Match':<10}")
print("-" * 110)

predictors = ["not-taken", "btfn", "1bit", "2bit"]

for t in bp_tests:
    out_oracle = run_sim(t, ["--batch", "--single-cycle"])
    regs_oracle = extract_regs(out_oracle)
    oracle_cycles = extract_batch_cycles(out_oracle)
    expected = golden_bp[t]

    if oracle_cycles != expected["retired"]:
        print(f"ERROR: Oracle cycle count {oracle_cycles} != expected retired {expected['retired']} for {t}")
        all_passed = False

    for p in predictors:
        out_p = run_sim(t, ["--batch", "--predictor", p])
        regs_p = extract_regs(out_p)
        stats = extract_bp_stats(out_p)

        reg_match = (regs_oracle == regs_p)
        if not reg_match:
            all_passed = False

        # Invariant 1: Register match
        # Invariant 2: retired == single-cycle cycles
        if stats["retired"] != expected["retired"]:
            print(f"ERROR: {t} {p} retired {stats['retired']} != expected {expected['retired']}")
            all_passed = False

        # Invariant 3: cycles == golden cycles
        if stats["cycles"] != expected["cycles"][p]:
            print(f"ERROR: {t} {p} cycles {stats['cycles']} != expected {expected['cycles'][p]}")
            all_passed = False

        # Invariant 4: mispredicts == golden mispredicts
        if stats["mispredicts"] != expected["mispredicts"][p]:
            print(f"ERROR: {t} {p} mispredicts {stats['mispredicts']} != expected {expected['mispredicts'][p]}")
            all_passed = False

        # Invariant 5: redirects = mispredicts + jalr
        if stats["redirects"] != (stats["mispredicts"] + stats["jalr"]):
            print(f"ERROR: {t} {p} redirects identity failed!")
            all_passed = False

        # Invariant 6: cycles = retired + 4 + 2*redirects + stalls
        if stats["cycles"] != (stats["retired"] + 4 + 2 * stats["redirects"] + stats["stalls"]):
            print(f"ERROR: {t} {p} cycle accounting identity failed!")
            all_passed = False

        print(f"{t:<25} | {p:<12} | {stats['cycles']:<6} | {stats['retired']:<7} | {stats['branches']:<8} | {stats['mispredicts']:<6} | {stats['jal']:<4} | {stats['jalr']:<4} | {('PASS' if reg_match else 'FAIL'):<10}")
    print("-" * 110)

if all_passed:
    print("\nSUCCESS: ALL LAB 1, 2, 3, AND 4 SPECIFICATIONS AND INVARIANTS PASS 100%!")
else:
    print("\nFAILURE: SOME TESTS DID NOT PASS GOLDEN SPECIFICATIONS!")
    sys.exit(1)
