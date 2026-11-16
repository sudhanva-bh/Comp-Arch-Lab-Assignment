import subprocess
import re

tests = [
    ("s1_alu", "sample_tests/lab1_skeleton/s1_alu.s", ["--batch"], 41, None),
    ("s2_memory", "sample_tests/lab1_skeleton/s2_memory.s", ["--batch"], 87, None),
    ("s3_control", "sample_tests/lab1_skeleton/s3_control.s", ["--batch"], 634, None),
    ("p1_broad", "sample_tests/lab2_pipeline/p1_broad.s", ["--pipeline-forward"], 91, 113),
    ("p2_edge", "sample_tests/lab2_pipeline/p2_edge.s", ["--pipeline-forward"], 43, 57),
    ("p3_kernel", "sample_tests/lab2_pipeline/p3_kernel.s", ["--pipeline-forward"], 658, 762),
    ("h1_broad", "sample_tests/lab3_hazards/h1_broad.s", ["--pipeline-forward"], 43, 52),
    ("h2_edge", "sample_tests/lab3_hazards/h2_edge.s", ["--pipeline-forward"], 30, 40),
    ("h3_kernel", "sample_tests/lab3_hazards/h3_kernel.s", ["--pipeline-forward"], 321, 461),
    ("b1_patterns", "sample_tests/lab4_branch/b1_patterns.s", ["--batch", "--predictor", "2bit"], 1330, 1476),
    ("b2_adversarial", "sample_tests/lab4_branch/b2_adversarial.s", ["--batch", "--predictor", "2bit"], 4739, 4985),
    ("b3_applied", "sample_tests/lab4_branch/b3_applied.s", ["--batch", "--predictor", "2bit"], 3195, 3982),
]

sim = "riscv_sim_base/rvsim.exe"

print(f"{'Test':<15} | {'Oracle':<7} | {'Exp Oracle':<10} | {'Pipe':<6} | {'Exp Pipe':<8} | {'Status':<6}")
print("-" * 65)

all_ok = True
for name, path, pipe_args, exp_oracle, exp_pipe in tests:
    res_oracle = subprocess.run([sim, path, "--batch", "--single-cycle"], stdout=subprocess.PIPE, text=True)
    m_oracle = re.search(r"cycles\s*=\s*(\d+)", res_oracle.stdout)
    oracle_cycles = int(m_oracle.group(1)) if m_oracle else None

    pipe_cycles = None
    if exp_pipe is not None:
        res_pipe = subprocess.run([sim, path] + pipe_args, stdout=subprocess.PIPE, text=True)
        m_pipe = re.search(r"cycles\s*=\s*(\d+)", res_pipe.stdout)
        pipe_cycles = int(m_pipe.group(1)) if m_pipe else None

    match_o = (oracle_cycles == exp_oracle)
    match_p = (pipe_cycles == exp_pipe) if exp_pipe is not None else True
    ok = match_o and match_p
    if not ok:
        all_ok = False
    status = "PASS" if ok else "FAIL"

    p_str = str(pipe_cycles) if pipe_cycles is not None else "-"
    ep_str = str(exp_pipe) if exp_pipe is not None else "-"
    print(f"{name:<15} | {oracle_cycles:<7} | {exp_oracle:<10} | {p_str:<6} | {ep_str:<8} | {status:<6}")

print("-" * 65)
if all_ok:
    print("ALL 12 TESTS MATCH 'Cycles references.md' AND GOLDEN SPECIFICATIONS EXACTLY!")
else:
    print("SOME TESTS FAILED!")
