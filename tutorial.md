# Tutorial: Build, Run, Test and Debug the Labs

This cookbook uses the source directory `riscv_sim_base/`. The four documentation
files are one level above it, at the repository root. Unless a section says
otherwise, commands after setup run **inside `riscv_sim_base`**. Shell prompts are
not part of commands. Examples show Linux paths; the PowerShell blocks use the
Windows executable name explicitly.

## 1. Build once and run a smoke test

### Linux / WSL / Bash

Install a C11 compiler, Make and Python 3 using your OS package manager. Then:

```bash
cd riscv_sim_base
make
./rvsim tests/sample1_sum_loop.s --single-cycle
make test
```

The sum program prints `55`, leaves t1/x6 and a0/x10 at `0x00000037`, and ends:

```text
status = ECALL_EXIT   cycles = 53
```

`make test` builds both verifier binaries and finishes with:

```text
PASS: 747 checks; W6-W9 goldens, counters, full architectural state, faults, limits, traces
```

It also runs the C predictor assertions. Build commands and `make` directory
messages may precede this line. To rerun the Python portion after a successful
build, use `python3 tests/run_tests.py`. Python alone does not build its binaries.

### Windows / PowerShell / MSVC

Open **Developer PowerShell for Visual Studio 2022**, with the C++ build tools
installed, and change to the extracted repository root. Build from source:

```powershell
Set-Location riscv_sim_base
$sources = Get-ChildItem core,isa,assembler,ui,pipeline -Filter *.c |
    ForEach-Object { $_.FullName }
$includes = @('/Icore','/Iisa','/Iassembler','/Iui','/Ipipeline')
cl /nologo /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS $includes $sources main.c /Fe:rvsim.exe
if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed' }
.\rvsim.exe tests\sample1_sum_loop.s --single-cycle
```

The Makefile is for GCC, not MSVC/NMake. Native MSVC execution was not verified
in the Linux development environment; these commands use the existing C11 source
layout. WSL is the recommended Windows route for the exact verified GCC suite.

To build native test binaries as well:

```powershell
cl /nologo /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS $includes $sources tests\check_state.c /Fe:tests\pipeline_checks.exe
if ($LASTEXITCODE -ne 0) { throw 'State verifier build failed' }
cl /nologo /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS $includes $sources tests\check_predictor.c /Fe:tests\predictor_checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Predictor verifier build failed' }
.\tests\predictor_checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Predictor assertions failed' }
python tests\run_tests.py
```

Use a Python 3 installation accessible as `python`; use `py -3` instead if that
is your installed launcher. The harness invokes the verifier and simulator paths;
Windows resolves their `.exe` counterparts. If your Windows toolchain/runtime
behaves differently, run the tested suite in WSL rather than treating an untested
native build as verified.

## 2. Understand the executable test inventory

The supplied samples already live in `tests/given/`. No copying or rewriting of
assembly is required. The original upload used separate lab folders and shell
harness names; this integrated project uses **one `tests/run_tests.py` harness**.
There is no `tests/run_tests.sh` in this implementation.

| Folder | Programs | Use |
|---|---|---|
| `tests/given/lab1_skeleton` | s1_alu, s2_memory, s3_control | Baseline single-cycle; safe pipelines also supported |
| `tests/given/lab2_pipeline` | p1_broad, p2_edge, p3_kernel | Reference no-hazard pipeline fixtures with scheduled spacing |
| `tests/given/lab3_hazards` | h1_broad, h2_edge, h3_kernel | Compare reference stall/forward; none is intentionally wrong |
| `tests/given/lab4_branch` | b1_patterns, b2_adversarial, b3_applied | All four predictors plus oracle |
| `tests/given/lab5_fp` | f1_broad, f2_special, f3_kernel | Reference-only until W10 is implemented |

All program names above end in `.s`. Labs 2–5 have `expected/` files containing
reference output. Only the 12 W6–W9 programs run in today's suite; all 15 are
listed in the benchmark matrix below. W10 assembly will currently be rejected,
not simulated successfully.

## 3. Lab 1 / W6: establish the baseline

**Goal:** confirm instruction meanings, register state and output before looking
at pipeline timing. Run all three baseline samples:

```bash
for f in tests/given/lab1_skeleton/*.s; do
    ./rvsim "$f" --single-cycle
done
```

```powershell
Get-ChildItem tests\given\lab1_skeleton\*.s | ForEach-Object {
    & .\rvsim.exe $_.FullName --single-cycle
}
```

Expected program-output strings are `2016`, `36`, and `755120`; cycle counts are
41, 87, and 634. The strings contain no separators between consecutive integer
print syscalls: `755120` is 7, then 55, then 120. Successful status is ECALL_EXIT,
and the single-cycle CPI is 1.000 even though the original engine does not print
a separate CPI summary.

To inspect one instruction interactively:

```bash
./rvsim tests/micro.s
```

```powershell
.\rvsim.exe tests\micro.s
```

At the `(sim)` prompt, type `step 1`, `regs`, `mem 0 20`, `status`, `help`, and
`quit` as needed. This debugger uses the original engine only. `break 0x10` and
`run` demonstrate a PC breakpoint; pipeline selectors instead imply batch mode.

## 4. Lab 2 / W7: observe overlapping stages

**Goal:** run the pipeline while preserving oracle state. For the sheet's original
sample1 comparison:

```bash
./rvsim tests/sample1_sum_loop.s --single-cycle
./rvsim tests/sample1_sum_loop.s --pipeline
```

```powershell
.\rvsim.exe tests\sample1_sum_loop.s --single-cycle
.\rvsim.exe tests\sample1_sum_loop.s --pipeline
```

Both print 55. The sheet-stalling run's actual summary is:

```text
status = ECALL_EXIT   cycles = 108
pipeline: mode=stall timing=sheet cycles=108 retired=53 stalls=40 flushes=11 CPI=2.038 fwd_ex=0 fwd_mem=0 load_use=0 discarded_stalls=0
```

Now run the supplied p1–p3 samples against their **reference pipeline** goldens:

```bash
for f in tests/given/lab2_pipeline/*.s; do
    ./rvsim "$f" --single-cycle
    ./rvsim "$f" --pipeline-basic
done
```

```powershell
Get-ChildItem tests\given\lab2_pipeline\*.s | ForEach-Object {
    & .\rvsim.exe $_.FullName --single-cycle
    & .\rvsim.exe $_.FullName --pipeline-basic
}
```

Reference pipeline cycles: p1=113, p2=57, p3=762; retired=91,43,658. p1/p2 print
nothing; p3 prints 1939. `--pipeline-basic` has no hazard handling, so the supplied
p* programs deliberately contain enough spacing. It is not safe for arbitrary
programs. Do not expect `--pipeline` to match these particular goldens: it uses a
different redirect cost and conservative stalling policy.

## 5. Lab 3 / W8: compare stall and forwarding

**Goal:** avoid waiting for a register write when its correct value already exists
in a pipeline latch. Run the five-instruction micro program:

```bash
./rvsim tests/micro.s --pipeline
./rvsim tests/micro.s --pipeline-forward
```

```powershell
.\rvsim.exe tests\micro.s --pipeline
.\rvsim.exe tests\micro.s --pipeline-forward
```

Expected final t0=5, t1=15, t2=20 in both runs. Summary fields:

| Mode | Cycles | Retired | Stalls | Flushes | CPI | EX/MEM forwards | MEM/WB forwards |
|---|---:|---:|---:|---:|---:|---:|---:|
| sheet stall | 15 | 5 | 6 | 0 | 3.000 | 0 | 0 |
| sheet forward | 9 | 5 | 0 | 0 | 1.800 | 2 | 1 |

For the supplied hazard suite use reference policies:

```bash
for f in tests/given/lab3_hazards/*.s; do
    ./rvsim "$f" --single-cycle
    ./rvsim "$f" --hazard stall
    ./rvsim "$f" --hazard forward
done
```

```powershell
Get-ChildItem tests\given\lab3_hazards\*.s | ForEach-Object {
    & .\rvsim.exe $_.FullName --single-cycle
    & .\rvsim.exe $_.FullName --hazard stall
    & .\rvsim.exe $_.FullName --hazard forward
}
```

| Sample | Prints | Retired | Stall cycles / stalls | Forward cycles / stalls | Forward CPI | Total forwarded operands |
|---|---|---:|---|---|---:|---:|
| h1_broad | 59 | 43 | 84 / 33 | 52 / 1 | 1.209 | 21 |
| h2_edge | 28 | 30 | 53 / 17 | 40 / 4 | 1.333 | 11 |
| h3_kernel | 1939 | 321 | 749 / 324 | 461 / 36 | 1.436 | 191 |

The remaining forward stalls are load-use stalls. Reference h1 prints:

```text
status = ECALL_EXIT   cycles = 52
pipeline: mode=forward timing=reference cycles=52 retired=43 stalls=1 flushes=2 CPI=1.209 fwd_ex=15 fwd_mem=6 load_use=1 discarded_stalls=0
```

Cycle reduction is $100(C_{stall}-C_{forward})/C_{stall}$: 38.1%, 24.5%, 38.5%
for h1,h2,h3. These differ from the sheet-original sample reductions because the
programs and timing policies differ.

Optional negative experiment: `./rvsim tests/given/lab3_hazards/h2_edge.s --hazard
none` (one terminal line) intentionally mishandles dependencies and eventually
reports ILLEGAL_INSTRUCTION in 37 cycles. Its expected file records that wrong
behavior; it is not a failure of the correct `stall`/`forward` modes. None mode
can issue unhandled-syscall warnings. Do not interpret faster wrong execution as
an optimization.

## 6. Lab 4 / W9: run every predictor

**Goal:** choose the likely next PC at fetch and recover correctly in EX.

```bash
for f in tests/given/lab4_branch/*.s; do
    ./rvsim "$f" --single-cycle
    for p in not-taken btfn 1bit 2bit; do
        ./rvsim "$f" --batch --predictor "$p"
    done
done
```

```powershell
Get-ChildItem tests\given\lab4_branch\*.s | ForEach-Object {
    $program = $_.FullName
    & .\rvsim.exe $program --single-cycle
    foreach ($predictor in @('not-taken','btfn','1bit','2bit')) {
        & .\rvsim.exe $program --batch --predictor $predictor
    }
}
```

The `--predictor` flag implies reference timing and forwarding. To use the safe
stalling policy instead:

```bash
./rvsim tests/given/lab4_branch/b3_applied.s --predictor 2bit --hazard stall
```

```powershell
.\rvsim.exe tests\given\lab4_branch\b3_applied.s --predictor 2bit --hazard stall
```

Never combine `--predictor` with `--pipeline-forward` or `--single-cycle`; the CLI
rejects that ambiguity. Run the oracle in a separate invocation.

| Sample | not-taken cycles / misses | btfn cycles / misses | 1bit cycles / misses | 2bit cycles / misses |
|---|---|---|---|---|
| b1_patterns | 1856 / 261 | 1432 / 49 | 1506 / 86 | 1476 / 71 |
| b2_adversarial | 5287 / 272 | 5007 / 132 | 5033 / 145 | 4985 / 121 |
| b3_applied | 3986 / 145 | 3986 / 145 | 3970 / 137 | 3982 / 143 |

For b1 with 2bit, output is 703233 and the exact summaries are:

```text
status = ECALL_EXIT   cycles = 1476
pipeline: mode=forward timing=reference cycles=1476 retired=1330 stalls=0 flushes=71 CPI=1.110 fwd_ex=337 fwd_mem=63 load_use=0 discarded_stalls=0
predictor: name=2bit branches=328 mispredicts=71 accuracy=78.4 jal=4 jalr=0 redirects=71 squashed=142
```

Accuracy in this line is a percentage, although the compact output omits the `%`
character. For b3/2bit the accounting is 3195+4+115+2×334=3982; 334 redirects are
143 misses plus 191 indirect jumps. Those indirect jumps cannot be improved by
changing this direction predictor. Do not attribute all control cycles to misses.

## 7. Master matrix: all 15 supplied sample programs

The RARS column below is **supplied reference data, not measured here**. Current
oracle and selected pipeline counts for s*/p*/h*/b* have been checked. FP rows
are **future reference targets only**. The selected pipeline is basic for p*,
reference-forward for h*, and reference-forward+2bit for b*. A dash means the
original cross-lab reference table did not assign a pipeline figure, not that
the current safe pipeline cannot run that integer sample.

| Program | Folder suffix under tests/given/ | Program prints | Supplied RARS | Oracle cycles | Selected pipeline cycles | Selected mode / availability |
|---|---|---|---:|---:|---:|---|
| s1_alu | lab1_skeleton | 2016 | 40 | 41 | — | --single-cycle |
| s2_memory | lab1_skeleton | 36 | 86 | 87 | — | --single-cycle |
| s3_control | lab1_skeleton | 755120 | 633 | 634 | — | --single-cycle |
| p1_broad | lab2_pipeline | none | 90 | 91 | 113 | --pipeline-basic |
| p2_edge | lab2_pipeline | none | cannot assemble | 43 | 57 | --pipeline-basic |
| p3_kernel | lab2_pipeline | 1939 | 657 | 658 | 762 | --pipeline-basic |
| h1_broad | lab3_hazards | 59 | 42 | 43 | 52 | --hazard forward |
| h2_edge | lab3_hazards | 28 | 29 | 30 | 40 | --hazard forward |
| h3_kernel | lab3_hazards | 1939 | 320 | 321 | 461 | --hazard forward |
| b1_patterns | lab4_branch | 703233 | 1329 | 1330 | 1476 | --predictor 2bit |
| b2_adversarial | lab4_branch | 4078 | 4738 | 4739 | 4985 | --predictor 2bit |
| b3_applied | lab4_branch | 697645586 | 3194 | 3195 | 3982 | --predictor 2bit |
| f1_broad | lab5_fp | 11 | 41 | 42* | 46* | W10 pending |
| f2_special | lab5_fp | 12 | 65 | 66* | 71* | W10 pending |
| f3_kernel | lab5_fp | 1432536311 | 196 | 197* | 222* | W10 pending |

`*` Reference target for an unimplemented FP engine. These three programs cannot
currently reproduce those figures. The supplied RARS/oracle counts use different
counting conventions; do not compare their raw numbers as a host-speed benchmark.
`Cycles references.md` and the provided expected files are the source of these
cross-tool figures. The original engine counts exit `ecall` as an instruction.

To run a single sample, combine its folder/name with its mode, for example:

```bash
./rvsim tests/given/lab3_hazards/h2_edge.s --hazard forward
```

```powershell
.\rvsim.exe tests\given\lab3_hazards\h2_edge.s --hazard forward
```

## 8. Compare results with the oracle and expected files

Do not diff whole dumps without normalization: PC, cycles, path headers and
statistics legitimately differ. For a quick register-only comparison:

```bash
./rvsim tests/given/lab3_hazards/h1_broad.s --single-cycle > ref.txt
./rvsim tests/given/lab3_hazards/h1_broad.s --hazard forward > got.txt
rg '^x' ref.txt > ref-regs.txt
rg '^x' got.txt > got-regs.txt
diff -u ref-regs.txt got-regs.txt
```

No diff output means the register lines match. If ripgrep is unavailable, use
`grep '^x'` in its place. On PowerShell:

```powershell
.\rvsim.exe tests\given\lab3_hazards\h1_broad.s --single-cycle > ref.txt
.\rvsim.exe tests\given\lab3_hazards\h1_broad.s --hazard forward > got.txt
$ref = Get-Content ref.txt | Where-Object { $_ -match '^x' }
$got = Get-Content got.txt | Where-Object { $_ -match '^x' }
Compare-Object $ref $got
```

Again, no output means equality. These quick comparisons do not prove RAM or
program output equality. For the full-state check of one chosen sample:

```bash
./tests/pipeline_checks tests/given/lab3_hazards/h1_broad.s
```

```powershell
.\tests\pipeline_checks.exe tests\given\lab3_hazards\h1_broad.s
```

The C verifier repeats the oracle and 12 safe pipeline configurations, compares
all 32 registers and all simulated memory bytes, and exits nonzero on mismatch.
Repeated program prints are normal because it executes the program repeatedly.
The Python harness suppresses those repeated prints during `make test`.

To inspect a supplied golden directly:

```bash
cat tests/given/lab4_branch/expected/b1_patterns.2bit.out
```

```powershell
Get-Content tests\given\lab4_branch\expected\b1_patterns.2bit.out
```

Golden summaries have aligned `name = value` fields. The current executable uses
compact `pipeline:` and `predictor:` lines. `run_tests.py` compares parsed values,
not formatting or the caller's source-path header; it also verifies printed
characters, status and cycle count. Use `make test` for those automatic golden
comparisons rather than a literal whole-file diff.

## 9. Read a clock trace

```bash
./rvsim tests/micro.s --pipeline-forward --trace > run.txt 2> trace.txt
./rvsim tests/regression/wrong_path_branch.s --predictor 2bit --trace > branch-run.txt 2> branch-trace.txt
```

```powershell
.\rvsim.exe tests\micro.s --pipeline-forward --trace 1> run.txt 2> trace.txt
.\rvsim.exe tests\regression\wrong_path_branch.s --predictor 2bit --trace 1> branch-run.txt 2> branch-trace.txt
Get-Content trace.txt
```

Micro forwarding trace excerpt, copied from the current executable:

```text
[cycle 3] IF 00000008 | ID *00000004 | EX *00000000 | MEM -00000000 | WB -00000000
[cycle 4] IF 0000000c | ID *00000008 | EX *00000004 | MEM *00000000 | WB -00000000
[cycle 5] IF 00000010 | ID *0000000c | EX *00000008 | MEM *00000004 | WB *00000000
```

Addresses are hexadecimal instruction PCs. `*` means a valid old latch; `-`
means invalid, so the address following `-` is stale metadata, not an active
instruction. At clock 4, EX at 0x04 consumes the result of 0x00 now in MEM.
At clock 5, EX at 0x08 uses the newer EX/MEM result and the older MEM/WB result.

The intended five-instruction schedule is:

```text
clock         1  2  3  4  5  6  7  8  9
0x00 addi    IF ID EX ME WB
0x04 addi       IF ID EX ME WB
0x08 add           IF ID EX ME WB
0x0c addi             IF ID EX ME WB
0x10 ecall               IF ID EX ME WB
```

For the stalling trace, use `--pipeline --trace`: the consumer PC repeats in ID,
`STALL` appears, and EX gets invalid bubbles. Fetch PC does not advance. This
trace's IF field is a fetch/attempt address, not a separate IF-valid latch marker;
while stalled or redirecting it may show the held/wrong-path fetch pointer.

The wrong-path branch regression gives:

```text
[cycle 4] IF 0000000c | ID *00000008 | EX *00000004 | MEM *00000000 | WB -00000000 FLUSH
[cycle 5] IF 00000010 | ID -00000008 | EX -00000004 | MEM *00000004 | WB *00000000
[cycle 6] IF 00000014 | ID *00000010 | EX -00000004 | MEM -00000004 | WB *00000004
```

At clock 4, branch 0x04 resolves taken; the initial weak-not-taken prediction was
wrong. IF's displayed 0x0c is discarded. Target 0x10 is fetched in clock 5, enters
ID in clock 6, and the two younger old-path slots never retire. Branch-count
summary must be 1, with jal=0: the younger branch/jump never execute in EX.

## 10. Cycle limits, faults and accounting

```bash
./rvsim tests/micro.s --pipeline-forward --max-cycles 3
./rvsim tests/regression/precise_memory.s --pipeline-forward
```

```powershell
.\rvsim.exe tests\micro.s --pipeline-forward --max-cycles 3
.\rvsim.exe tests\regression\precise_memory.s --pipeline-forward
```

The first remains RUNNING with cycles=3 and a reached-limit message. The second
reports MEM_FAULT; its older s0 write survives while younger state changes are
suppressed. Fetch/decode faults and RAM effects are checked by the C verifier.

For normal completion, use sheet `cycles=retired+stalls+flushes+4` or reference
`cycles=retired+stalls+2*flushes+4`. W9 also has
`redirects=mispredicts+jalr`. Retired includes exit, excludes bubbles and squashes.
Subtract `discarded_stalls` for documented younger stalls overlapping terminal
drain. Do not force these equations onto faulted or artificially limited runs.

A CPU fault or cycle-limit message does not necessarily produce a nonzero host
process exit code: main returns zero after execution. Read the status line. The
test harness checks status explicitly.

## 11. Five common debugging failures

| Symptom | Likely cause | What to inspect / remedy |
|---|---|---|
| Consumer calculates with an old value, especially after repeated writes | Wrong bypass priority or missing operand path | Inspect `forward_one`: EX/MEM must win over MEM/WB; include stores, branches and jalr, not just ALU inputs |
| A load consumer calculates using an address | EX/MEM load address forwarded as data, or missing interlock | Require `!mem_read` for EX/MEM result forwarding; stall an immediate load consumer once |
| Instructions disappear or a stalled program hangs | IF/ID overwritten during stall, PC advances, invalid producer included | Hold IFID and PC, inject EX bubble; qualify producers by valid/reg_write/nonzero rd |
| Illegal-instruction failure before a valid exit, or a wrong-path store changes RAM | Fault raised too early or incomplete squash / terminal ordering | Carry fault to WB; redirect beats ID; terminal WB returns before younger MEM stores |
| Registers match but cycles, misses or forwards differ | Wrong timing mode, bubble retirement, counter wrap or speculative counting | Check CLI mode, count valid retirement, saturate two-bit counters; use nominal redirect cost and terminal overlap metadata |

Before debugging semantics, also check the working directory, whether `make`
rebuilt changed headers, and the program's final status. A full-file golden diff
can look wrong solely because the display layout and fetch PC differ.

## 12. Sanitizer reproduction and cleanup

On Linux/WSL with GCC sanitizer support, from `riscv_sim_base`:

```bash
make clean
make test CFLAGS='-MMD -MP -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Icore -Iisa -Iassembler -Iui -Ipipeline' LDFLAGS='-fsanitize=address,undefined'
make clean
make test
```

The final clean/rebuild restores normal binaries: Make does not track changes to
command-line compiler flags. Clean before switching build configurations.
The verified sanitizer suite passes the same 747 checks and C assertions.

## 13. W10: inspect the fixtures, do not claim they pass

You may read future samples and their expected output:

```bash
cat tests/given/lab5_fp/f1_broad.s
cat tests/given/lab5_fp/expected/f1_broad.forward.out
```

```powershell
Get-Content tests\given\lab5_fp\f1_broad.s
Get-Content tests\given\lab5_fp\expected\f1_broad.forward.out
```

Do not run an all-directories wildcard and interpret rejected FP mnemonics as a
W6–W9 regression. The harness deliberately excludes `lab5_fp`. Once the FP state,
CSR and numerical helpers, register metadata, third-operand paths and 26 handlers
exist, extend verification to x registers, f registers, fcsr, memory and output.
See [the W10 plan](COMPREHENSIVE_LAB_GUIDE.md#8-lab-5-specification-infrastructure-before-rv32f-handlers).
