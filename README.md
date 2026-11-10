# RV32I Processor Simulator: Single-Cycle & 5-Stage Pipeline

A modular, teaching-oriented **cycle-accurate RV32I RISC-V processor simulator** written in C11. This project spans four progressive computer architecture lab assignments:

- **Lab 1 (Week 6): Baseline Single-Cycle RV32I Simulator** — Models architectural state (`CPUState`), byte-addressable memory (`Memory`), instruction decoding, two-pass assembly, and dynamic dispatch via a central ISA registry (`core/isa_registry.c`).
- **Lab 2 (Week 7): 5-Stage Pipelined Simulator (Stalling & Flushing)** — Slices instruction execution across 5 synchronous stages (**IF, ID, EX, MEM, WB**) using clock latches (`IF/ID`, `ID/EX`, `EX/MEM`, `MEM/WB`), a snapshot/commit execution model, control signal generation, naive 3-latch stalling for RAW hazards, and branch flushes.
- **Lab 3 (Week 8): Data Hazards, Forwarding & Load-Use Interlocks** — Introduces an autonomous Hazard Detection Unit and a Forwarding Unit. Forwards ALU results directly from `EX/MEM` and `MEM/WB` into EX inputs, eliminating almost all stalls while enforcing an unavoidable 1-cycle stall for true load-use hazards.
- **Lab 4 (Week 9): Branch Prediction & Pre-Decode Unit** — Integrates branch direction prediction (`not-taken`, `btfn`, `1bit`, `2bit`), pre-decodes `jal` in the IF stage (making unconditional direct jumps completely free with 0 penalty cycles), evaluates conditional branches in EX with a 2-cycle penalty on mispredict, penalizes `jalr` by 2 cycles (no RAS), and implements cycle-by-cycle `--trace` logging.

---

## 1. Directory Structure

```text
Lab Assignment/
├── README.md                      # Master architecture and execution guide
├── verify_all.py                  # Automated test and oracle verification suite (Labs 1-4)
├── W6_RV32I_Processor_Simulator.md# Lab 1 notes (transcribed slides)
├── W7_RISCV_Pipeline_Lab_Sheet.md # Lab 2 handout and specifications
├── W8_Lab_Sheet_3_Data_Hazards.md # Lab 3 handout and specifications
├── W9_Lab Sheet 4 Branch Prediction on the Pipelined RV32I Simulator.md # Lab 4 handout and specs
└── riscv_sim_base/
    ├── Makefile                   # Build configuration with dynamic predictor targets
    ├── main.c                     # Program entry point and CLI option routing
    ├── core/                      # Core architectural models (untouched)
    │   ├── cpu_state.h/.c         # Registers x0..x31 (x0 hardwired to 0), PC, status
    │   ├── memory.h/.c            # 1 MB flat byte-addressable little-endian RAM
    │   ├── isa_registry.h/.c      # Instruction dispatch table (single source of truth)
    │   ├── decoder.h/.c           # Bit-field extraction & sign-extended immediates
    │   └── executor.h/.c          # Single-cycle execution loop (Reference Oracle)
    ├── isa/
    │   └── rv32i_base.h/.c        # RV32I instruction handlers (add, sub, lw, sw, beq...)
    ├── assembler/
    │   └── assembler.h/.c         # Two-pass assembler (resolves labels & produces machine code)
    ├── pipeline/                  # Modular pipeline & branch prediction implementation
    │   ├── control.h/.c           # ControlSignals struct and control_decode()
    │   ├── pipeline_state.h       # IFID, IDEX, EXMEM, MEMWB latches & Pipeline struct
    │   ├── hazard.h/.c            # HazardVerdict, produces(), hazard_check(), forward_operands()
    │   ├── branch_predictor.h/.c  # BranchPredictor struct, BPState (64 entries), predictor registry
    │   ├── bp_static.c            # Static predictors: 'not-taken' and 'btfn'
    │   ├── bp_dynamic.c           # Dynamic predictors: '1bit' and '2bit' saturating counters
    │   └── pipeline.c             # 5-stage pipeline_cycle() & pipeline_run_batch()
    ├── ui/
    │   └── cli_display.h/.c       # CLI debugger REPL and register dump routines
    └── tests/                     # Test assembly programs
        ├── micro.s                # 5-instruction RAW dependency benchmark (Lab 3 §4)
        ├── sample1_sum_loop.s     # Sum 1 to 10 loop benchmark (53 instructions)
        ├── sample2_mem_and_branches.s # Load/store and branch comparison benchmark
        ├── load_use_test.s        # Comprehensive 4-case load-use hazard verification
        ├── t1_counted_loop.s      # Lab 4 single loop (9 taken, 1 not-taken backward branch)
        ├── t2_nested_loops.s      # Lab 4 nested loops (hysteresis benchmark)
        ├── t3_alternating.s       # Lab 4 alternating T/N forward branches
        └── t4_calls.s             # Lab 4 nested calls, returns, JALs, and load-use stalls
```

---

## 2. Prerequisites & Building

### Prerequisites
- GCC with C11 support (`gcc` / MinGW-w64 on Windows)
- GNU `make`
- Python 3.7+ (for running `verify_all.py`)

### Compilation
From the project root:
```bash
cd riscv_sim_base
make clean
make
```
This builds all core components, ISA handlers, assembler, pipeline logic, and branch predictors into `rvsim` (`rvsim.exe` on Windows).

---

## 3. How to Run

### A. Lab 1 — Single-Cycle Mode (`--batch` or interactive REPL)
Runs 1 instruction per cycle ($\text{CPI} = 1.0$). This engine serves as the **Reference Oracle** across all labs.
```bash
# Batch mode (runs to completion and prints register dump)
./rvsim tests/sample1_sum_loop.s --batch

# Interactive debugger (supports 'step', 'run', 'regs', 'mem', 'break')
./rvsim tests/sample1_sum_loop.s
```

### B. Lab 2 — 5-Stage Pipelined Mode with Naive Stalling (`--pipeline`)
Executes the five stages concurrently. Detects RAW hazards across a 3-latch window (`ID/EX`, `EX/MEM`, `MEM/WB`) and stalls by freezing IF and ID while injecting a bubble into EX. Branches are evaluated in EX and flush younger instructions on taken conditions.
```bash
./rvsim tests/sample1_sum_loop.s --pipeline
```

### C. Lab 3 — 5-Stage Pipelined Mode with Data Forwarding (`--pipeline-forward`)
Activates operand bypassing from `EX/MEM` (distance 1) and `MEM/WB` (distance 2) directly into EX ALU inputs, store data, branch comparisons, and `jalr` target calculations. Detects load-use dependencies and injects an unavoidable 1-cycle stall.
```bash
./rvsim tests/load_use_test.s --pipeline-forward
```

### D. Lab 4 — Branch Prediction & Trace Modes
Specifies a branch prediction policy (`not-taken`, `btfn`, `1bit`, `2bit`). JAL instructions are pre-decoded in IF (free), conditional branches mispredict with a 2-cycle penalty, and `jalr` returns cost 2 cycles:
```bash
# Run with static 'not-taken' predictor
./rvsim tests/t1_counted_loop.s --batch --predictor not-taken

# Run with static 'btfn' (backward taken, forward not-taken)
./rvsim tests/t1_counted_loop.s --batch --predictor btfn

# Run with dynamic '1bit' predictor
./rvsim tests/t1_counted_loop.s --batch --predictor 1bit

# Run with dynamic '2bit' saturating counter predictor
./rvsim tests/t1_counted_loop.s --batch --predictor 2bit

# Run with cycle-by-cycle 5-stage ASCII pipeline trace
./rvsim tests/t1_counted_loop.s --batch --predictor 2bit --trace
```

### Additional Command-Line Options
- `--hazard <stall|forward>`: Selects hazard unit policy (default: `forward` when `--predictor` is specified, otherwise `stall` under `--pipeline`).
- `--single-cycle`: Explicitly runs the single-cycle execution engine.
- `--max-cycles <N>`: Sets the maximum cycle limit (default: 1,000,000).
- `--mem-size <N>`: Configures simulated memory size in bytes (default: 1,048,576 = 1 MB).

---

## 4. Benchmark Results & Verification Standards

### A. Labs 1, 2, and 3 Pipeline Verification

| Program | Oracle Cycles | Lab 2 Cycles (Stalling) | Lab 3 Cycles (Forwarding) | Forwarding Speedup | Oracle Reg Match |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `micro.s` | 5 | 15 (6 stalls, 0 flushes) | 9 (0 stalls, 0 flushes) | **1.67x** | **100% MATCH** |
| `sample1_sum_loop.s` | 53 | 119 (40 stalls, 11 flushes) | 79 (0 stalls, 11 flushes) | **1.51x** | **100% MATCH** |
| `sample2_mem_and_branches.s` | 13 | 27 (8 stalls, 1 flush) | 19 (0 stalls, 1 flush) | **1.42x** | **100% MATCH** |
| `load_use_test.s` | 13 | 37 (18 stalls, 1 flush) | 23 (4 load-use stalls, 1 flush) | **1.61x** | **100% MATCH** |

#### Pipeline Clock Accounting Identity:
$$\text{cycles} = \text{retired} + 4 + \text{stalls} + 2 \times \text{flushes}$$

---

### B. Lab 4 Branch Prediction Reference Benchmark (W9 §11 Golden Matrix)

| Program | Retired | Single-Cycle | not-taken | btfn | 1bit | 2bit |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `t1_counted_loop.s` | 37 | 37 | 59 (9 misses) | 43 (1 miss) | 45 (2 misses) | 45 (2 misses) |
| `t2_nested_loops.s` | 52 | 52 | 78 (11 misses) | 66 (5 misses) | 76 (10 misses) | 70 (7 misses) |
| `t3_alternating.s` | 50 | 50 | 76 (11 misses) | 64 (5 misses) | 74 (10 misses) | 74 (10 misses) |
| `t4_calls.s` | 219 | 219 | 278 (2 misses) | 278 (2 misses) | 280 (3 misses) | 278 (2 misses) |
| **Total Cycles** | **358** | **358** | **491** | **451** | **475** | **467** |
| **Overall CPI** | **1.000** | **1.000** | **1.372** | **1.260** | **1.327** | **1.305** |
| **Total Misses (62 branches)**| - | - | **33 (46.8% acc)** | **13 (79.0% acc)**| **25 (59.7% acc)**| **21 (66.1% acc)**|

#### Lab 4 Branch Prediction Clock Accounting Identity:
$$\text{cycles} = \text{retired} + 4 + 2 \times \text{redirects} + \text{stalls}$$
$$\text{redirects} = \text{mispredicts} + \text{jalr}$$

---

## 5. Automated Verification Suite

Run the master test runner from the root directory:
```bash
python verify_all.py
```
This tests:
1. **Lab 1-3 Suites**: Verifies register file equivalence across single-cycle, stalling, and forwarding modes, and validates the clock cycle accounting identities.
2. **Lab 4 Suite**: Verifies that every single predictor (`not-taken`, `btfn`, `1bit`, `2bit`) matches the golden metrics (cycles, retired count, branches, mispredicts, JAL, JALR) and preserves register file fidelity against the single-cycle oracle.
