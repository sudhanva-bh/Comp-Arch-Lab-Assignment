# RISC-V RV32I Processor Simulator: Complete Lab Tutorial

A comprehensive, step-by-step practical guide to understanding, building, running, and verifying the **RV32I Cycle-Accurate Processor Simulator** across all laboratory assignments (Labs 1 through 5).

---

## Table of Contents
1. [Overview & Architectural Evolution](#1-overview--architectural-evolution)
2. [Prerequisites & Building the Simulator](#2-prerequisites--building-the-simulator)
3. [Lab 1 (Week 6): Baseline Single-Cycle RV32I Simulator](#3-lab-1-week-6-baseline-single-cycle-rv32i-simulator)
4. [Lab 2 (Week 7): 5-Stage Pipelined Simulator (Stalling & Flushing)](#4-lab-2-week-7-5-stage-pipelined-simulator-stalling--flushing)
5. [Lab 3 (Week 8): Data Hazards, Forwarding & Load-Use Interlocks](#5-lab-3-week-8-data-hazards-forwarding--load-use-interlocks)
6. [Lab 4 (Week 9): Branch Prediction & Pre-Decode Unit](#6-lab-4-week-9-branch-prediction--pre-decode-unit)
7. [Lab 5 (Week 10): Floating-Point Arithmetic Extension (RV32F)](#7-lab-5-week-10-floating-point-arithmetic-extension-rv32f)
8. [Master Verification & Benchmark Reference](#8-master-verification--benchmark-reference)
9. [Debugging & Common Pitfalls](#9-debugging--common-pitfalls)

---

## 1. Overview & Architectural Evolution

The processor simulator is designed as a progressive hardware modeling experiment in C11:
- **Core Invariant**: Functional correctness never changes. Regardless of pipelining, forwarding, or branch prediction, **the final register state of every program must match the Single-Cycle Oracle bit-for-bit**.
- **Performance Invariant**: Optimizations alter *when* instructions execute (cycles and CPI), never *what* they compute.

```text
+-----------------------------------------------------------------------------------+
| Lab 1 (W6): Single-Cycle Engine (CPI = 1.0, Reference Oracle)                     |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
| Lab 2 (W7): 5-Stage Pipeline (IF, ID, EX, MEM, WB) + Naive 3-Latch RAW Stalls     |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
| Lab 3 (W8): Data Forwarding (EX/MEM & MEM/WB) + 1-Cycle Load-Use Interlock        |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
| Lab 4 (W9): Branch Prediction (not-taken, btfn, 1bit, 2bit) + Free JAL Pre-Decode |
+-----------------------------------------+-----------------------------------------+
                                          |
                                          v
+-----------------------------------------------------------------------------------+
| Lab 5 (W10): Floating-Point RV32F (fregs[32], fcsr, 26 FP instructions)          |
+-----------------------------------------------------------------------------------+
```

---

## 2. Prerequisites & Building the Simulator

### Directory Structure
```text
Lab Assignment/
├── tutorial.md                    # This complete guide
├── verify_all.py                  # Automated test runner for Labs 1-4
├── check_sample_tests.py          # Sample test runner matching Cycles references.md
├── Cycles references.md           # Golden cycle matrix across all 15 sample tests
├── sample_tests/                  # 15 reference programs (lab1 to lab5)
└── riscv_sim_base/                # Simulator source code
    ├── core/                      # Architectural state, decoder, memory, oracle
    ├── isa/                       # RV32I instruction handlers
    ├── assembler/                 # Two-pass assembler (supports labels, pseudo-ops, 'la')
    ├── pipeline/                  # 5-stage pipeline, hazard unit, branch predictors
    └── ui/                        # Interactive REPL & register dumper
```

### Compiling on Windows
To compile a native 64-bit binary using MSVC from PowerShell or Command Prompt:

```powershell
cmd.exe /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cd /d "s:\BITSH\3-1\Comp Arch\Lab Assignment\riscv_sim_base" && cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Icore /Iisa /Iassembler /Iui /Ipipeline core\*.c isa\*.c assembler\*.c pipeline\*.c ui\*.c main.c /Fe:rvsim.exe'
```

### Compiling on Linux / macOS
```bash
cd riscv_sim_base
make clean
make
```

The resulting executable is `rvsim.exe` (Windows) or `rvsim` (Linux/macOS).

---

## 3. Lab 1 (Week 6): Baseline Single-Cycle RV32I Simulator

### The Goal
Model the architectural state and sequential execution of an RV32I processor:
1. **CPU State**: 32 general-purpose registers (`x0` through `x31`), where `x0` is hardwired to 0, plus `pc` and `next_pc`.
2. **Memory**: Flat 1 MB little-endian byte-addressable memory.
3. **Dynamic Dispatch**: `core/isa_registry.c` acts as a shared instruction registry mapping machine bitfields to C function pointers without giant switch statements.
4. **Execution Model**: One instruction executes completely per clock cycle ($\text{CPI} = 1.000$). This serves as the **Reference Oracle** for all subsequent labs.

### How to Run

#### Interactive Debugger REPL
Explore execution step-by-step:
```powershell
.\riscv_sim_base\rvsim.exe sample_tests\lab1_skeleton\s1_alu.s
```
**REPL Commands:**
- `step` (or `s`): Execute 1 instruction and print stage info.
- `run` (or `r`): Run to completion or breakpoint.
- `regs`: Dump all 32 integer registers in hex.
- `mem <hex_addr> <count>`: Inspect memory contents.
- `break <hex_addr>`: Set a breakpoint at an address.
- `quit` (or `q`): Exit REPL.

#### Batch Mode (Oracle Execution)
```powershell
# Run arithmetic benchmark
.\riscv_sim_base\rvsim.exe sample_tests\lab1_skeleton\s1_alu.s --batch

# Explicit single-cycle mode
.\riscv_sim_base\rvsim.exe sample_tests\lab1_skeleton\s2_memory.s --batch --single-cycle
```

### What to Expect
- **`s1_alu.s`**:
  - Console prints: `2016`
  - Total Cycles: `41`
  - Final State: `status = ECALL_EXIT cycles = 41`
- **`s2_memory.s`**:
  - Console prints: `36`
  - Total Cycles: `87`
- **`s3_control.s`**:
  - Console prints: `755120` (7, 55, then 120)
  - Total Cycles: `634`

---

## 4. Lab 2 (Week 7): 5-Stage Pipelined Simulator (Stalling & Flushing)

### The Goal
Convert the sequential machine into a 5-stage synchronous pipeline:
1. **Stages**: **IF** (Fetch), **ID** (Decode & Register Read), **EX** (Execute & Branch), **MEM** (Memory Access), **WB** (Register Writeback & Exceptions).
2. **Latches**: Connect stages through pipeline registers (`IF/ID`, `ID/EX`, `EX/MEM`, `MEM/WB`).
3. **Snapshot/Commit Model**: Each cycle reads old latch contents (`p->...`) and writes next state (`nx....`).
4. **Control Signals**: Generated in ID via `control_decode()` and carried down the pipeline.
5. **Naive Stalling**: Any RAW dependence within the 3 downstream latches stalls the consumer in ID for 1 to 3 cycles (freezes IF and ID, injects a bubble into EX).
6. **Branch Handling**: Evaluated in EX; if taken, flushes the two younger instructions in IF and ID.

### How to Run
```powershell
# Run with naive 3-latch stalling pipeline
.\riscv_sim_base\rvsim.exe riscv_sim_base\tests\sample1_sum_loop.s --pipeline
```

### What to Expect
- **Stall Penalty**: Heavy execution stalls because every back-to-back RAW dependency costs 3 stall cycles.
- **Clock Accounting Identity**:
  $$\text{cycles} = \text{retired} + \text{stalls} + 2 \times \text{flushes} + 4$$
- **Example Metrics (`sample1_sum_loop.s`)**:
  - Single-Cycle: 53 cycles
  - Pipeline Mode: 119 cycles (`retired = 53`, `stalls = 40`, `flushes = 11`, `CPI = 2.245`)
  - Register Match: **100% Identical to Single-Cycle Oracle**.

---

## 5. Lab 3 (Week 8): Data Hazards, Forwarding & Load-Use Interlocks

### The Goal
Eliminate unnecessary pipeline stalls by forwarding computed results directly between stages:
1. **Forwarding Paths**:
   - `EX/MEM.alu_result` $\to$ EX stage inputs (Distance 1, highest priority).
   - `MEM/WB.wb_val` $\to$ EX stage inputs (Distance 2).
2. **Wired Consumers in EX**:
   - ALU operand `a` (from `rs1`) and operand `b` (from `rs2`).
   - Store data (`store_val = o.b`).
   - Branch comparison inputs (`branch_taken(..., o.a, o.b)`).
   - `jalr` target address calculation (`(o.a + imm) & ~1u`).
3. **Load-Use Hazard Detection**:
   - A load instruction does not produce its data until the end of MEM.
   - If an instruction in ID reads the register that a load currently in `ID/EX` writes, it **must stall for exactly 1 cycle**.
   - After 1 stall, the load is in MEM/WB, and the standard forwarding path supplies the value to EX.

### How to Run
```powershell
# Run with data forwarding enabled
.\riscv_sim_base\rvsim.exe riscv_sim_base\tests\micro.s --pipeline-forward

# Compare against stalling mode
.\riscv_sim_base\rvsim.exe riscv_sim_base\tests\micro.s --pipeline
```

### What to Expect
- **Performance Speedup**:
  - `micro.s` (5 instructions with back-to-back RAW):
    - Stalling: 15 cycles (6 stalls, CPI 3.00)
    - Forwarding: **9 cycles (0 stalls, CPI 1.80) — 1.67x speedup!**
- **Load-Use Verification (`load_use_test.s`)**:
  - Exercises load $\to$ ALU, load $\to$ branch, load $\to$ store, and load $\to$ load.
  - Forwarding mode takes 23 cycles (exactly 4 load-use stalls, 1 branch flush).
- **Banner Output**:
  ```text
  pipeline: mode=forward cycles=52 retired=43 stalls=1 flushes=2 CPI=1.209 fwd_ex=13 fwd_mem=5 load_use=1
  ```

---

## 6. Lab 4 (Week 9): Branch Prediction & Pre-Decode Unit

### The Goal
Mitigate control hazard penalties using pre-decoding and branch direction prediction:
1. **Free JAL**: In the IF stage, unconditional direct jumps (`jal`) are pre-decoded from the opcode and immediate bits. IF redirects itself immediately (**0 penalty cycles**).
2. **Direction Predictors**:
   - `not-taken`: Static; always predicts fall-through (`pc + 4`).
   - `btfn`: Static; Backward Taken (`target < pc`), Forward Not-Taken (`target >= pc`).
   - `1bit`: Dynamic; 64-entry table indexed by `(pc >> 2) & 0x3F` remembering the last outcome.
   - `2bit`: Dynamic; 64-entry 2-bit saturating counter ($0 \leftrightarrow 1 \leftrightarrow 2 \leftrightarrow 3$). Starts weakly not-taken (`1`), predicts taken if state $\ge 2$.
3. **Execution & Penalties**:
   - Prediction occurs in IF.
   - Verification occurs in EX. On mispredict, EX flushes IF and ID (**2 penalty cycles**).
   - Indirect jumps (`jalr`) cannot be pre-decoded or predicted; always incur **2 penalty cycles**.
4. **Clock Accounting Identity**:
   $$\text{cycles} = \text{retired} + 4 + 2 \times \text{redirects} + \text{stalls}$$
   $$\text{redirects} = \text{mispredicts} + \text{jalr}$$

### How to Run
```powershell
# Run with static predictors
.\riscv_sim_base\rvsim.exe sample_tests\lab4_branch\b1_patterns.s --batch --predictor not-taken
.\riscv_sim_base\rvsim.exe sample_tests\lab4_branch\b1_patterns.s --batch --predictor btfn

# Run with dynamic predictors
.\riscv_sim_base\rvsim.exe sample_tests\lab4_branch\b1_patterns.s --batch --predictor 1bit
.\riscv_sim_base\rvsim.exe sample_tests\lab4_branch\b1_patterns.s --batch --predictor 2bit

# Run with cycle-by-cycle ASCII pipeline trace
.\riscv_sim_base\rvsim.exe riscv_sim_base\tests\t1_counted_loop.s --batch --predictor 2bit --trace
```

### What to Expect
- **Benchmark Summary Table (W9 Section 11 Golden Matrix)**:

| Program | Retired | Single-Cycle | `not-taken` | `btfn` | `1bit` | `2bit` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `t1_counted_loop.s` | 37 | 37 | 59 (9 misses) | **43 (1 miss)** | 45 (2 misses) | 45 (2 misses) |
| `t2_nested_loops.s` | 52 | 52 | 78 (11 misses) | 66 (5 misses) | 76 (10 misses) | **70 (7 misses)** |
| `t3_alternating.s`  | 50 | 50 | 76 (11 misses) | **64 (5 misses)**| 74 (10 misses) | 74 (10 misses) |
| `t4_calls.s`        | 219 | 219 | 278 (2 misses)| 278 (2 misses)| 280 (3 misses)| 278 (2 misses) |

- **Key Takeaways**:
  - `btfn` wins on single loops without needing any history tables.
  - `2bit` beats `1bit` on nested loops (`t2`) because hysteresis prevents exiting loops from thrashing the prediction state on re-entry.
  - In `t4_calls.s`, 33 `jal` instructions are free (saving 66 cycles), while 18 `jalr` returns cost $18 \times 2 = 36$ cycles regardless of predictor.
- **Trace Output Example**:
  ```text
  [cycle 7] IF 0x00000018 | ID 0x00000014 | EX 0x00000010 | MEM 0x0000000c | WB 0x00000008
  [cycle 8] IF 0x00000008 | ID ---------- | EX ---------- | MEM 0x00000010 | WB 0x0000000c
  ```
  *(Cycle 8 shows IF restarting at `0x0008` while IF/ID are squashed with bubbles `----------` due to a mispredict).*

---

## 7. Lab 5 (Week 10): Floating-Point Arithmetic Extension (RV32F)

### The Goal
Extend the architecture with single-precision IEEE 754 floating point entirely through the instruction registry without redesigning the pipeline:
1. **Architectural State**:
   - 32 floating-point registers `f0` through `f31` (`f0` is an ordinary register, not hardwired to zero).
   - Control and status register `fcsr` holding rounding mode `frm` (bits 7:5) and sticky exception flags `NV, DZ, OF, UF, NX` (bits 4:0).
2. **26 Instructions**:
   - **Data Transfer & Bit Manipulation**: `flw`, `fsw`, `fsgnj.s`, `fsgnjn.s`, `fsgnjx.s`, `fmv.x.w`, `fmv.w.x`, `fclass.s`.
   - **Arithmetic**: `fadd.s`, `fsub.s`, `fmul.s`, `fdiv.s`, `fsqrt.s`, and 4 fused multiply-adds (`fmadd.s`, `fmsub.s`, `fnmsub.s`, `fnmadd.s`).
   - **Comparisons & Conversions**: `feq.s` (quiet), `flt.s` (signalling), `fle.s` (signalling), `fmin.s`, `fmax.s`, `fcvt.w.s`, `fcvt.wu.s`, `fcvt.s.w`, `fcvt.s.wu`.
3. **Pipeline Integration**:
   - Unified register numbers: `x0..x31` mapped to `0..31`, `f0..f31` mapped to `32..63`.
   - Existing hazard and forwarding units automatically handle FP dependencies without modifications.
   - Fused multiply-adds use the third source operand `src3` / `fwd3`.

### Implementation Requirements (To Be Added)
- Implement `isa/rv32f.c`, `isa/fp_helpers.c`, `isa/zicsr.c`.
- Update `CPUState` in `core/cpu_state.h` with `fregs[32]` and `fcsr`.
- Register RV32F instructions in `main.c` via `register_rv32f()`.

---

## 8. Master Verification & Benchmark Reference

### Automated Verification Scripts

Two test runners are provided in the workspace:

#### 1. Invariant & Pipeline Verification (`verify_all.py`)
Validates that Labs 1 to 4 satisfy all mathematical identities and match golden values:
```powershell
python verify_all.py
```

#### 2. Sample Tests Verification (`check_sample_tests.py`)
Runs all 12 implemented integer benchmark programs from `sample_tests` and verifies them against [Cycles references.md](file:///s:/BITSH/3-1/Comp%20Arch/Lab%20Assignment/Cycles%20references.md):
```powershell
python check_sample_tests.py
```

### Complete Reference Cycles Matrix

| Test Name | Suite | Program Path | RARS Cycles | Oracle Cycles | Pipeline Cycles (Forward / 2bit) | Printed Output |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: |
| `s1_alu` | Lab 1 | `sample_tests/lab1_skeleton/s1_alu.s` | 40 | 41 | — | `2016` |
| `s2_memory` | Lab 1 | `sample_tests/lab1_skeleton/s2_memory.s` | 86 | 87 | — | `36` |
| `s3_control` | Lab 1 | `sample_tests/lab1_skeleton/s3_control.s` | 633 | 634 | — | `755120` |
| `p1_broad` | Lab 2 | `sample_tests/lab2_pipeline/p1_broad.s` | 90 | 91 | 113 | *(Registers)* |
| `p2_edge` | Lab 2 | `sample_tests/lab2_pipeline/p2_edge.s` | *N/A* | 43 | 57 | *(Registers)* |
| `p3_kernel` | Lab 2 | `sample_tests/lab2_pipeline/p3_kernel.s` | 657 | 658 | 762 | `1939` |
| `h1_broad` | Lab 3 | `sample_tests/lab3_hazards/h1_broad.s` | 42 | 43 | 52 | `59` |
| `h2_edge` | Lab 3 | `sample_tests/lab3_hazards/h2_edge.s` | 29 | 30 | 40 | `28` |
| `h3_kernel` | Lab 3 | `sample_tests/lab3_hazards/h3_kernel.s` | 320 | 321 | 461 | `1939` |
| `b1_patterns` | Lab 4 | `sample_tests/lab4_branch/b1_patterns.s` | 1329 | 1330 | 1476 | `703233` |
| `b2_adversarial` | Lab 4 | `sample_tests/lab4_branch/b2_adversarial.s` | 4738 | 4739 | 4985 | `4078` |
| `b3_applied` | Lab 4 | `sample_tests/lab4_branch/b3_applied.s` | 3194 | 3195 | 3982 | `697645586` |
| `f1_broad` | Lab 5 | `sample_tests/lab5_fp/f1_broad.s` | 41 | 42 | 46 | *(Lab 5)* |
| `f2_special` | Lab 5 | `sample_tests/lab5_fp/f2_special.s` | 65 | 66 | 71 | *(Lab 5)* |
| `f3_kernel` | Lab 5 | `sample_tests/lab5_fp/f3_kernel.s` | 196 | 197 | 222 | *(Lab 5)* |

---

## 9. Debugging & Common Pitfalls

1. **`x0` Hardwiring**:
   - `x0` must always evaluate to `0`. Writes to `x0` must be discarded.
   - Handled in `cpu_write_reg()` and `produces()` (`rd != 0`).
2. **Snapshot / Commit Rule in Pipelining**:
   - Every stage must read from the current latch (`p->...`) and write into the next latch (`nx....`).
   - If a stage writes directly into `p->...`, future stages observe updated state within the same cycle (time travel).
3. **Stall Actions Must Be Complete**:
   - A pipeline stall in ID must do three things:
     1. Inject bubble into EX: `nx.id_ex.valid = 0`
     2. Freeze IF/ID: `nx.if_id = p->if_id`
     3. Freeze PC fetch: `if (!stall) cpu->pc = fetch_pc + 4`
4. **2-Bit Saturating Counter Bounds**:
   - Dynamic 2-bit counters must saturate between 0 and 3 (`*c < 3 ? (*c)++ : 3`). Never let counters wrap around from 3 to 0 or 0 to 3.
5. **Precise Exception Handling**:
   - Instructions after an illegal fetch or illegal decode must carry the fault through the pipeline registers. Only when the faulting instruction reaches **WB** does the CPU stop and report the error.
