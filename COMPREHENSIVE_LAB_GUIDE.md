# Comprehensive RV32I Processor Simulator Guide: Architecture, Labs & Implementation Details

Welcome to the **RV32I Processor Simulator** repository! This document serves as the complete, pedagogical, and implementation-level guide to the codebase. It is designed to take a first-time reader from the fundamental principles of computer architecture to the exact line-by-line mechanics of our cycle-accurate RISC-V simulator.

---

## Table of Contents
1. [Big Picture Context & Architectural Objectives](#1-big-picture-context--architectural-objectives)
2. [Codebase Organization & Separation of Concerns](#2-codebase-organization--separation-of-concerns)
3. [Lab 1 (Week 6) — Baseline RV32I Processor Simulator](#3-lab-1-week-6--baseline-rv32i-processor-simulator)
4. [Lab 2 (Week 7) — 5-Stage Pipelined RV32I Simulator (Stalling & Flushing)](#4-lab-2-week-7--5-stage-pipelined-rv32i-simulator-stalling--flushing)
5. [Lab 3 (Week 8) — Data Hazards, Forwarding Unit & Load-Use Interlocks](#5-lab-3-week-8--data-hazards-forwarding-unit--load-use-interlocks)
6. [Lab 4 (Week 9) — Branch Prediction & Control Hazard Mitigation](#6-lab-4-week-9--branch-prediction--control-hazard-mitigation)
7. [The Verification Framework & Golden Oracles](#7-the-verification-framework--golden-oracles)
8. [First-Time User Walkthrough & Commands Reference](#8-first-time-user-walkthrough--commands-reference)

---

## 1. Big Picture Context & Architectural Objectives

### 1.1 What Are We Building?
The central goal of this project is to build an educational, cycle-accurate processor simulator in C11 implementing the open **RISC-V 32-bit Integer Base Instruction Set Architecture (RV32I)**.

Rather than implementing a monolithic or opaque simulator, this project follows an incremental, four-stage evolution:
1. **Single-Cycle Functional Engine:** Fetches, decodes, and executes exactly one instruction per clock cycle ($\text{CPI} = 1.0$). This engine acts as our **Reference Oracle**; it defines exact mathematical correctness.
2. **5-Stage Synchronous Pipeline:** Slices instruction processing across the classic RISC stages (**IF, ID, EX, MEM, WB**) using pipeline registers (clock latches) and a snapshot/commit clock model.
3. **Data Hazard Resolution & Forwarding:** Eliminates unnecessary execution bubbles by detecting Read-After-Write (RAW) data dependencies and bypassing ALU results directly between pipeline latches, while enforcing an unavoidable 1-cycle stall on true load-use hazards.
4. **Speculative Execution & Branch Prediction:** Overcomes control hazards by pre-decoding unconditional jumps in the Fetch stage and employing static and dynamic branch direction predictors (`not-taken`, `btfn`, `1bit`, `2bit`) to minimize branch penalty flushes.

### 1.2 Why Build It Sequentially?
In real hardware, software does not run on an idealized machine where every instruction completes instantly. Instructions overlap in time. When instructions overlap, they compete for data (Data Hazards) and path decisions (Control Hazards).

By building the simulator sequentially:
- We first ensure the instruction semantics, memory models, and assembly decoding are 100% bug-free (Lab 1).
- We slice the datapath into synchronous stages, observing the staggering performance penalty of naive stalling (Lab 2).
- We implement hardware bypass paths to recover execution throughput (Lab 3).
- We implement branch prediction tables to address pipeline squashing (Lab 4).
- Most importantly: **At every single lab stage, the final register state of every program must match the Single-Cycle Oracle bit-for-bit.** Performance optimizations alter *when* instructions execute, never *what* they compute.

---

## 2. Codebase Organization & Separation of Concerns

The repository enforces a strict separation between immutable framework components and lab-specific extension modules.

```text
Lab Assignment/
├── README.md                      # High-level overview and execution guide
├── COMPREHENSIVE_LAB_GUIDE.md     # This comprehensive architectural reference
├── verify_all.py                  # Master test suite (validates Labs 1-4 invariants)
├── W6_RV32I_Processor_Simulator.md# Lab 1 specifications
├── W7_RISCV_Pipeline_Lab_Sheet.md # Lab 2 specifications
├── W8_Lab_Sheet_3_Data_Hazards.md # Lab 3 specifications
├── W9_Lab Sheet 4 Branch Prediction on the Pipelined RV32I Simulator.md # Lab 4 specifications
└── riscv_sim_base/
    ├── Makefile                   # C11 build configuration
    ├── main.c                     # Program entry point, flag parser & engine router
    ├── core/                      # CORE ENGINE (Untouched throughout all labs)
    │   ├── cpu_state.h/.c         # Architectural registers x0..x31, PC, status
    │   ├── memory.h/.c            # 1 MB little-endian flat RAM, load/store routines
    │   ├── isa_registry.h/.c      # Dynamic instruction dispatch table (Single source of truth)
    │   ├── decoder.h/.c           # 32-bit bitfield decoding & immediate extraction
    │   └── executor.h/.c          # Single-cycle fetch/decode/execute loop (Reference Oracle)
    ├── isa/
    │   └── rv32i_base.h/.c        # RV32I instruction handlers (add, sub, lw, sw, beq...)
    ├── assembler/
    │   └── assembler.h/.c         # Two-pass assembler (resolves labels, emits machine code)
    ├── ui/
    │   └── cli_display.h/.c       # CLI debugger, register dumper, memory inspector
    ├── pipeline/                  # PIPELINE ENGINE (Labs 2, 3, and 4)
    │   ├── control.h/.c           # Control signal decoding (RegWrite, MemRead, ALUOp...)
    │   ├── pipeline_state.h       # IFID, IDEX, EXMEM, MEMWB latches & Pipeline struct
    │   ├── hazard.h/.c            # Hazard unit (naive stall, load-use, forwarding logic)
    │   ├── branch_predictor.h/.c  # Branch predictor interface & registry
    │   ├── bp_static.c            # 'not-taken' and 'btfn' static predictors
    │   ├── bp_dynamic.c           # '1bit' and '2bit' saturating counter dynamic predictors
    │   └── pipeline.c             # 5-stage pipeline_cycle(), snapshot/commit & trace
    └── tests/                     # Verification test programs
        ├── micro.s                # RAW dependency micro-benchmark
        ├── sample1_sum_loop.s     # Sum 1..10 loop benchmark (53 instructions)
        ├── sample2_mem_and_branches.s # Memory access and branching benchmark
        ├── load_use_test.s        # 4-case load-use hazard interlock verification
        ├── t1_counted_loop.s      # Lab 4 single counted loop benchmark
        ├── t2_nested_loops.s      # Lab 4 nested loops benchmark
        ├── t3_alternating.s       # Lab 4 alternating branch pattern benchmark
        └── t4_calls.s             # Lab 4 nested calls, returns, and jumps benchmark
```

### The Architectural Plugin Paradigm: `isa_registry`
In typical amateur simulators, instructions are parsed via giant, fragile `switch(opcode)` blocks. Here, instructions are **plugins**. 

Every instruction registers itself in `core/isa_registry.c` with:
- Mnemonic string (e.g., `"add"`)
- Opcode, funct3, and funct7 bit patterns
- Instruction format (`FMT_R`, `FMT_I`, `FMT_S`, `FMT_B`, `FMT_U`, `FMT_J`)
- Function pointer to an execution handler: `void (*execute)(CPUState *cpu, Memory *mem, const DecodedInstr *d)`

Because the assembler and the simulator both query `isa_registry`, adding a new instruction (or an entire extension like RV32M or RV32F) requires creating one file and registering it. The core decoder, assembler, and pipeline never need to be modified.

---

## 3. Lab 1 (Week 6) — Baseline RV32I Processor Simulator

### 3.1 Problem Statement & Objectives
The goal of Lab 1 is to construct a functional, single-cycle RV32I architectural simulator.
- Emulate the 32 architectural registers (`x0` through `x31`), enforcing the architectural invariant that register `x0` is hardwired to zero (writes to `x0` must be discarded).
- Emulate a 1 MB flat, byte-addressable, little-endian memory space.
- Provide a two-pass assembler capable of translating assembly text (`.text`, `.data`, labels, pseudo-ops) into 32-bit RISC-V machine words.
- Provide a single-cycle execution engine where each clock tick executes one full instruction:
  $$\text{PC} \to \text{Memory Fetch} \to \text{Decode} \to \text{Execute Handler} \to \text{PC Update}$$

### 3.2 Key Tasks
1. **Instruction Decoding (`core/decoder.c`):**
   Extract the 7-bit opcode (`bits[6:0]`), destination register `rd` (`bits[11:7]`), `funct3` (`bits[14:12]`), source register 1 `rs1` (`bits[19:15]`), source register 2 `rs2` (`bits[24:20]`), and `funct7` (`bits[31:25]`).
2. **Immediate Sign-Extension:**
   Immediates in RISC-V are scrambled across instruction formats to keep register specifier bit positions constant. The decoder must reconstruct sign-extended 32-bit values:
   - **I-type:** `imm[11:0] = raw[31:20]`
   - **S-type:** `imm[11:0] = {raw[31:25], raw[11:7]}`
   - **B-type:** `imm[12:0] = {raw[31], raw[7], raw[30:25], raw[11:8], 1'b0}`
   - **U-type:** `imm[31:12] = raw[31:12]`
   - **J-type:** `imm[20:0] = {raw[31], raw[19:12], raw[20], raw[30:21], 1'b0}`
3. **Execution Loop (`core/executor.c`):**
   In single-cycle mode, `executor_step()` executes one instruction, updates the architectural program counter (`cpu->pc`), increments `cpu->cycle_count`, and returns the CPU status (`CPU_RUNNING`, `CPU_HALTED`, `CPU_EXITED`, etc.).

### 3.3 Implementation Details
In `core/cpu_state.c`:
```c
void cpu_write_reg(CPUState *cpu, uint32_t reg, uint32_t val) {
    if (reg == 0) return; /* x0 is hardwired to 0 */
    if (reg < 32) cpu->regs[reg] = val;
}

uint32_t cpu_read_reg(const CPUState *cpu, uint32_t reg) {
    if (reg == 0) return 0;
    return (reg < 32) ? cpu->regs[reg] : 0;
}
```
In `core/executor.c`:
```c
CPUStatus executor_step(CPUState *cpu, Memory *mem) {
    uint32_t raw_word;
    if (memory_read_word(mem, cpu->pc, &raw_word) != MEM_OK) {
        cpu->status = CPU_MEM_FAULT;
        return cpu->status;
    }
    DecodedInstr d;
    const InstructionHandler *h = NULL;
    DecodeStatus ds = decode_instruction(raw_word, &d, &h);
    if (ds != DECODE_OK) {
        cpu->status = CPU_ILLEGAL_INSTRUCTION;
        return cpu->status;
    }
    uint32_t next_pc = cpu->pc + 4;
    cpu->pc = next_pc; // Handler may overwrite cpu->pc on branch/jump
    h->execute(cpu, mem, &d);
    cpu->cycle_count++;
    return cpu->status;
}
```

---

## 4. Lab 2 (Week 7) — 5-Stage Pipelined RV32I Simulator (Stalling & Flushing)

### 4.1 Problem Statement & Objectives
Single-cycle processors are throughput-limited by the slowest instruction (usually `lw`, which traverses fetch, decode, ALU, memory, and writeback in one long clock period). Pipelining breaks this execution into five synchronous stages:
1. **IF (Instruction Fetch):** Read 32-bit instruction from memory at `PC`. Speculatively compute $\text{PC} + 4$.
2. **ID (Instruction Decode):** Decode instruction fields, extract control signals, and read register operands `rs1_val` and `rs2_val`.
3. **EX (Execute):** Perform ALU arithmetic/logical operations, evaluate branch conditions, or compute memory effective addresses.
4. **MEM (Memory Access):** Read memory on loads (`lw`, `lh`, `lb`) or write memory on stores (`sw`, `sh`, `sb`).
5. **WB (Write Back):** Write the result (from ALU or Memory) back into architectural register `rd`.

### 4.2 Challenges in Hardware Emulation
1. **Clock Edge Synchronicity (The Snapshot/Commit Model):**
   In a physical hardware clock cycle, all pipeline registers update simultaneously at the rising clock edge. In C, sequentially executing stages can create data races (e.g., if WB writes a value that ID immediately reads in the same simulator call).
   - **Solution:** We maintain two complete pipeline states: `*p` (the current state at clock cycle $t$) and `nx` (the next state being prepared for clock cycle $t+1$). Stages only read from `*p` and write into `nx`. At the end of the cycle, we commit: `*p = nx`.
2. **Control Signals Generation (`pipeline/control.c`):**
   The decoder must generate control bits so downstream stages know what to do:
   - `reg_write`: 1 if this instruction writes to `rd`.
   - `mem_read`: 1 if `lw`.
   - `mem_write`: 1 if `sw`.
   - `is_branch`: 1 if conditional branch (`beq`, `bne`, etc.).
   - `is_jump`: 1 if unconditional jump (`jal`, `jalr`).
   - `uses_rs1`, `uses_rs2`: 1 if register operands are read.
3. **Naive Data Hazard Handling (3-Latch Stalling):**
   In Lab 2, there is no forwarding unit. If instruction $I_1$ writes to register $x5$ and subsequent instructions $I_2, I_3, I_4$ read $x5$, a Read-After-Write (RAW) hazard occurs. Because writeback happens in the WB stage (5th stage), a dependent instruction in ID cannot proceed until $I_1$ has cleared WB!
   - The naive hazard detection unit inspects `ID/EX`, `EX/MEM`, and `MEM/WB`. If any latch contains a pending write to a register needed by the instruction in ID:
     - **Freeze PC** (do not fetch a new instruction).
     - **Freeze IF/ID** (keep the dependent instruction in ID to re-decode next cycle).
     - **Inject Bubble into ID/EX** (`valid = 0` / NOP into EX).
4. **Control Hazards (Branch Flushes):**
   In Lab 2, branch directions and jump targets are calculated in the EX stage. By the time a branch reaches EX, two younger instructions have already been speculatively fetched into IF and ID! If the branch is taken:
   - Squash `IF/ID` and `ID/EX` (`valid = 0`).
   - Redirect PC to the branch target.
   - Cost: **2 penalty cycles** per taken branch/jump.

### 4.3 Implementation Details
In `pipeline/pipeline_state.h`:
```c
typedef struct {
    int valid;
    uint32_t pc;
    DecodedInstr instr;
    ControlSignals ctrl;
    uint32_t rs1_val, rs2_val;
    // ...
} IDEX;

typedef struct {
    IFID if_id;
    IDEX id_ex;
    EXMEM ex_mem;
    MEMWB mem_wb;
    int forwarding;          /* 0 = Lab 2 naive stall, 1 = Lab 3 forwarding */
    uint64_t cycles, retired, stalls, flushes;
    // ...
} Pipeline;
```

In `pipeline/hazard.c`:
```c
int produces(int valid, const ControlSignals *lc, uint32_t rd, uint32_t rs) {
    return valid && lc->reg_write && rd != 0 && rd == rs;
}

HazardVerdict hazard_check(const Pipeline *p, const DecodedInstr *d, const ControlSignals *c) {
    HazardVerdict v = {0, 0};
    if (!p->forwarding) {
        uint32_t s1 = c->uses_rs1 ? d->rs1 : 0;
        uint32_t s2 = c->uses_rs2 ? d->rs2 : 0;
        // Check all 3 downstream pipeline latches:
        if (produces(p->id_ex.valid,  &p->id_ex.ctrl,  p->id_ex.instr.rd,  s1) ||
            produces(p->id_ex.valid,  &p->id_ex.ctrl,  p->id_ex.instr.rd,  s2) ||
            produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, s1) ||
            produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, s2) ||
            produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, s1) ||
            produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, s2)) {
            v.stall = 1;
        }
        return v;
    }
    // ...
}
```

#### Lab 2 Clock Accounting Identity
Every cycle in the pipeline is mathematically accounted for:
$$\text{cycles} = \text{retired} + 4 + \text{stalls} + 2 \times \text{flushes}$$
Where:
- `retired`: Total useful instructions that committed in the WB stage.
- `4`: Pipeline fill latency (cycles for the first instruction to traverse from IF to WB).
- `stalls`: Total 1-cycle bubbles injected due to data hazards.
- `2 * flushes`: 2 cycles wasted for each taken branch or jump that squashed IF and ID.

---

## 5. Lab 3 (Week 8) — Data Hazards, Forwarding Unit & Load-Use Interlocks

### 5.1 Problem Statement & Objectives
Lab 2's naive stalling ruins performance. For example, in `micro.s`:
```assembly
addi t0, zero, 5    # I1: writes t0 in WB (cycle 5)
addi t1, t0, 10     # I2: needs t0 in EX (cycle 3) -> stalls 3 cycles!
add  t2, t1, t0     # I3: needs t1 and t0 -> stalls 3 cycles!
```
5 instructions take 15 cycles ($\text{CPI} = 3.0$).

However, the result of $I_1$ is already calculated at the end of the EX stage! It sits right inside the `EX/MEM` latch. It does not need to travel through MEM, write into the register file in WB, and be re-read in ID.
- **Goal:** Implement a **Forwarding Unit (Bypass Network)** that routes computed values directly into the EX stage ALU inputs.
- **Goal:** Implement **Load-Use Hazard Detection**. Loaded data from memory is only available *after* the MEM stage. If an instruction immediately consumes a loaded value, forwarding from EX cannot supply it in time; the hardware *must* insert a mandatory 1-cycle stall.

### 5.2 Implementation of Forwarding Paths
There are two primary forwarding paths:
1. **EX Hazard (Distance 1):**
   Forward from `EX/MEM.alu_result` to ALU input A or B.
   Condition:
   `p->ex_mem.valid && p->ex_mem.ctrl.reg_write && p->ex_mem.instr.rd != 0 && p->ex_mem.instr.rd == id_ex.rs1`
2. **MEM Hazard (Distance 2):**
   Forward from `MEM/WB.wb_val` to ALU input A or B.
   Condition:
   `p->mem_wb.valid && p->mem_wb.ctrl.reg_write && p->mem_wb.instr.rd != 0 && p->mem_wb.instr.rd == id_ex.rs1`
3. **The Priority Rule (Most Recent Value Wins):**
   If both `EX/MEM` and `MEM/WB` write to the same register $x$, `EX/MEM` is the younger, more recent instruction and **must take priority**.

In `pipeline/hazard.c`:
```c
Operands forward_operands(const Pipeline *p, const IDEX *d) {
    Operands o = { d->rs1_val, d->rs2_val, 0, 0 };
    if (!p->forwarding || d->fault != PF_NONE) return o;

    // Check rs1
    if (d->ctrl.uses_rs1 && d->instr.rs1 != 0) {
        if (produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, d->instr.rs1)) {
            o.a = p->ex_mem.alu_result;
            o.fwd_ex++;      /* Distance 1: EX/MEM wins */
        } else if (produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, d->instr.rs1)) {
            o.a = p->mem_wb.wb_val;
            o.fwd_mem++;     /* Distance 2: MEM/WB */
        }
    }

    // Check rs2
    if (d->ctrl.uses_rs2 && d->instr.rs2 != 0) {
        if (produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, d->instr.rs2)) {
            o.b = p->ex_mem.alu_result;
            o.fwd_ex++;
        } else if (produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, d->instr.rs2)) {
            o.b = p->mem_wb.wb_val;
            o.fwd_mem++;
        }
    }
    return o;
}
```

### 5.3 The Load-Use Hazard Interlock
Consider:
```assembly
lw   t0, 0(sp)     # In EX stage: computing address sp + 0. Data not in register yet!
addi t1, t0, 1     # In ID stage: needs t0 for ALU addition in the very next cycle!
```
Even with forwarding, the data from memory is not read until the end of MEM stage. When `addi` is in EX, `lw` is in MEM, so the data is only available at the *end* of that cycle.
The Hazard Detection Unit in ID detects this:
```c
if (p->id_ex.valid && p->id_ex.ctrl.mem_read && p->id_ex.instr.rd != 0 &&
    ((c->uses_rs1 && d->rs1 == p->id_ex.instr.rd) ||
     (c->uses_rs2 && d->rs2 == p->id_ex.instr.rd))) {
    v.stall = 1;
    v.load_use = 1;
}
```
When triggered:
- Freeze PC and `IF/ID`.
- Insert 1 bubble into `ID/EX`.
- In the subsequent cycle, `lw` has advanced to `MEM/WB`, allowing `MEM/WB` forwarding to seamlessly provide the memory data into EX!
- Cost: exactly **1 cycle stall**.

---

## 6. Lab 4 (Week 9) — Branch Prediction & Control Hazard Mitigation

### 6.1 Problem Statement & Objectives
Even with full data forwarding, control hazards remain a major bottleneck. Every taken branch or jump squashes 2 stages (`IF` and `ID`), wasting 2 clock cycles. In loops and function-heavy code, branches can constitute 20% or more of instructions, artificially capping CPI above 1.4.

Lab 4 introduces two major architectural enhancements:
1. **Instruction Fetch Pre-Decoding for `jal`:**
   In RISC-V, the unconditional jump `jal rd, imm` has its 20-bit relative offset embedded directly in the instruction word. The target does not depend on any register values! By pre-decoding bits `[6:0] == 0x6F` right in the IF stage, the fetch unit computes the jump target immediately and redirects PC on clock cycle 0.
   - Result: **0 penalty cycles** for `jal`!
2. **Branch Direction Prediction:**
   Conditional branches (`beq`, `bne`, `blt`, `bge`, etc.) depend on ALU register comparisons that cannot be known in IF. The IF stage queries a **Branch Predictor** to guess whether the branch will be taken or not taken:
   - **If predicted correctly:** 0 penalty cycles! Execution proceeds without interruption.
   - **If mispredicted:** EX detects the mismatch, squashes the 2 speculatively fetched instructions in IF and ID, and redirects PC to the correct target. Penalty: **2 cycles**.
3. **Indirect Jumps (`jalr`):**
   `jalr rd, rs1, imm` computes its target from a register (`rs1 + imm`). Because register values are not available in IF (and this design does not include a Return Address Stack), `jalr` is never predicted and always incurs a 2-cycle redirect penalty.

### 6.2 The Branch Predictor Framework
All branch predictors conform to a clean interface in `pipeline/branch_predictor.h`:
```c
typedef struct {
    uint8_t table[64]; /* 64-entry prediction table */
} BPState;

typedef struct BranchPredictor {
    const char *name;
    const char *desc;
    void (*reset)(BPState *state);
    int  (*predict)(const BPState *state, uint32_t pc, uint32_t target);
    void (*update)(BPState *state, uint32_t pc, uint32_t target, int taken);
} BranchPredictor;

static inline size_t bp_index(uint32_t pc) {
    return (pc >> 2) & 0x3F; /* pc[7:2]: 64 entries */
}
```

### 6.3 The Four Implemented Predictors

#### 1. `not-taken` (Static Baseline)
- **Policy:** Always predict NOT TAKEN (0).
- **Rationale:** Traditional baseline. Works well on conditional checks that rarely trigger, but mispredicts on every taken loop iteration.

#### 2. `btfn` (Backward Taken, Forward Not-Taken)
- **Policy:** Look at address arithmetic:
  `int predict_btfn(..., uint32_t pc, uint32_t target) { return target < pc; }`
- **Rationale:** Backward branches (`target < pc`) are loop back-edges, which are taken almost every iteration. Forward branches (`target > pc`) are conditional skips/error exits, which are usually not taken. Requires 0 bits of state memory!

#### 3. `1bit` (Dynamic Last-Outcome Table)
- **Policy:** Maintain a 64-entry table of single bits.
  - Predict: `table[bp_index(pc)]`
  - Update: `table[bp_index(pc)] = taken ? 1 : 0`
- **Weakness:** Cannot handle loop re-entry well. On loop exit, it gets updated to 0. When the loop is re-entered, it predicts 0 on the first iteration and misses again (2 misses per loop execution).

#### 4. `2bit` (Dynamic Saturating Counters)
- **Policy:** 64 entries of 2-bit counters initialized to `1` (**weakly not-taken**):
  - `0`: Strongly Not-Taken (Predict N)
  - `1`: Weakly Not-Taken (Predict N)
  - `2`: Weakly Taken (Predict T)
  - `3`: Strongly Taken (Predict T)
  - Prediction threshold: `counter >= 2` (top bit is prediction, bottom bit is confidence).
  - Update rule: On taken, increment counter up to saturation maximum 3 (do not wrap!). On not-taken, decrement counter down to 0 (do not wrap!).
- **Why it wins:** Hysteresis! A single anomaly (such as a loop exit) only shifts the counter from 3 to 2. When the loop is re-entered, the counter is still $\ge 2$, so it correctly predicts TAKEN on the very first iteration, saving a cycle.

### 6.4 Lab 4 Clock Accounting Identity
In Lab 4, the definition of redirects becomes explicit:
$$\text{cycles} = \text{retired} + 4 + 2 \times \text{redirects} + \text{stalls}$$
$$\text{redirects} = \text{mispredicts} + \text{jalr}$$
Notice that `jal` does not appear anywhere in the penalty equation because IF pre-decoding completely eliminated its penalty!

---

## 7. The Verification Framework & Golden Oracles

To guarantee that the simulator is bulletproof, our repository includes a comprehensive Python verification harness: `verify_all.py`.

### 7.1 Verification Strategy
The verification harness executes tests across multiple pipeline configurations and enforces two tiers of checks:
1. **Tier 1 — The Architectural Register Invariant:**
   A branch predictor or pipeline hazard unit modifies *timing*, never *correctness*. The register dump ($x0..x31$) at program termination under `--pipeline`, `--pipeline-forward`, and all four `--predictor` modes must be **100% bit-for-bit identical** to the Single-Cycle Oracle (`--batch --single-cycle`).
2. **Tier 2 — Performance & Clock Cycle Invariants:**
   Every run must strictly satisfy the clock accounting equations and match the golden reference cycle and mispredict counts from the lab handouts.

### 7.2 Golden Results Matrix

#### Part 1: Labs 1, 2, and 3 Pipeline Performance

| Program | Single-Cycle (Oracle) | Lab 2 (Stalling) | Lab 3 (Forwarding) | Forwarding Speedup | Register Match |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `micro.s` | 5 | 15 (6 stalls, 0 flushes) | 9 (0 stalls, 0 flushes) | **1.67x** | **100% PASS** |
| `sample1_sum_loop.s` | 53 | 119 (40 stalls, 11 flushes) | 79 (0 stalls, 11 flushes) | **1.51x** | **100% PASS** |
| `sample2_mem_and_branches.s` | 13 | 27 (8 stalls, 1 flush) | 19 (0 stalls, 1 flush) | **1.42x** | **100% PASS** |
| `load_use_test.s` | 13 | 37 (18 stalls, 1 flush) | 23 (4 load-use stalls, 1 flush) | **1.61x** | **100% PASS** |

#### Part 2: Lab 4 Branch Prediction Reference Benchmark (W9 §11 Matrix)

| Program | Retired | Single-Cycle | not-taken | btfn | 1bit | 2bit |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `t1_counted_loop.s` | 37 | 37 | 59 (9 misses) | 43 (1 miss) | 45 (2 misses) | 45 (2 misses) |
| `t2_nested_loops.s` | 52 | 52 | 78 (11 misses) | 66 (5 misses) | 76 (10 misses) | 70 (7 misses) |
| `t3_alternating.s` | 50 | 50 | 76 (11 misses) | 64 (5 misses) | 74 (10 misses) | 74 (10 misses) |
| `t4_calls.s` | 219 | 219 | 278 (2 misses) | 278 (2 misses) | 280 (3 misses) | 278 (2 misses) |
| **Total Cycles** | **358** | **358** | **491** | **451** | **475** | **467** |
| **Overall CPI** | **1.000** | **1.000** | **1.372** | **1.260** | **1.327** | **1.305** |
| **Accuracy (62 branches)** | — | — | **46.8% (33 misses)** | **79.0% (13 misses)** | **59.7% (25 misses)** | **66.1% (21 misses)** |

---

## 8. First-Time User Walkthrough & Commands Reference

### 8.1 Building the Project
From the repository root or inside `riscv_sim_base/`:
```bash
cd riscv_sim_base
make clean
make
```
This builds the binary `rvsim` (`rvsim.exe` on Windows).

### 8.2 Running Different Lab Modes

#### 1. Interactive Step-by-Step Debugger (Lab 1)
```bash
./rvsim tests/sample1_sum_loop.s
```
Commands inside the REPL:
- `step <N>`: Advance $N$ instructions.
- `run`: Run until completion or breakpoint.
- `regs`: Display all 32 general-purpose registers in hex.
- `mem <hex_addr> <count>`: Inspect raw memory bytes.
- `break <hex_addr>`: Set an execution breakpoint.
- `help`: View available debugger commands.

#### 2. Single-Cycle Oracle Batch Mode (Lab 1)
```bash
./rvsim tests/sample1_sum_loop.s --batch
```

#### 3. Pipelined Mode with Naive Stalling (Lab 2)
```bash
./rvsim tests/sample1_sum_loop.s --pipeline
```

#### 4. Pipelined Mode with Data Forwarding (Lab 3)
```bash
./rvsim tests/sample1_sum_loop.s --pipeline-forward
```

#### 5. Pipelined Mode with Branch Prediction (Lab 4)
```bash
# Static Not-Taken predictor
./rvsim tests/t1_counted_loop.s --batch --predictor not-taken

# Static BTFN predictor
./rvsim tests/t1_counted_loop.s --batch --predictor btfn

# Dynamic 1-bit table predictor
./rvsim tests/t1_counted_loop.s --batch --predictor 1bit

# Dynamic 2-bit saturating counter predictor
./rvsim tests/t1_counted_loop.s --batch --predictor 2bit
```

#### 6. Cycle-by-Cycle ASCII Pipeline Trace (Lab 4)
To see instructions flowing through `IF | ID | EX | MEM | WB` in real-time:
```bash
./rvsim tests/t1_counted_loop.s --batch --predictor 2bit --trace
```
Example trace output snippet:
```text
[cycle 7]  IF 0x00000010 | ID 0x0000000c | EX 0x00000008 | MEM 0x00000004 | WB 0x00000000
[cycle 8]  IF 0x00000014 | ID 0x00000010 | EX 0x0000000c | MEM 0x00000008 | WB 0x00000004
```

### 8.3 Running the Full Verification Test Suite
From the repository root:
```bash
python verify_all.py
```
This runs every program across all pipeline and predictor configurations, validating register dumps against the Single-Cycle Oracle, checking clock accounting identities, and ensuring CPI and misprediction counts match the reference lab specifications.

