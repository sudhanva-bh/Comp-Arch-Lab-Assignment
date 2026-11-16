# Software as Silicon: Demystifying the RV32I Processor Simulator

*Transcribed from the 15-page image-only slide deck. The original pages contain diagrams and rendered text; their text is reproduced below, with diagrams described rather than embedded.*

## Slide 1 — Software as Silicon: Demystifying the RV32I Processor Simulator

Using C programming to model architectural state, dynamic dispatch, and the RISC-V execution cycle.

**Detailed image/diagram description:** A conceptual schematic shows a software box containing `handler->execute(cpu, mem, &instr);` on the left and a trapezoidal ALU on the right. Three wires feed the ALU: **REGISTER INPUT (A)**, **REGISTER INPUT (B)**, and **CONTROL SIGNAL (OPCODE)**. An output line labeled **DATA OUTPUT** leads to **RESULT (to Register/Memory)**. The diagram visually equates a C instruction handler with physical computational hardware.

---

## Slide 2 — The Core Philosophy: Modularity

Semantics — What it is — Mnemonic (`add`), fields (`rs1`, `rs2`, `rd`).
Encoding — Its physical shape — 32-bit machine code format (Opcode, Funct3, Funct7).
Decoding — How we read it — Bitwise extraction and masking logic.
Execution — What it does — The mapped behavior (ALU operations, Memory access).

Modular Core Architecture

The brilliance of this codebase is its modularity. By separating what an instruction looks like from how it executes, the code remains clean, extensible, and perfect for teaching.

**Detailed image/diagram description:** Four side-by-side, differently outlined vertical columns labeled **Semantics**, **Encoding**, **Decoding**, and **Execution** each have a title, brief conceptual description, and illustrative content. All four feed a wide bottom block labeled **Modular Core Architecture**, showing that those four responsibilities jointly make up the simulator.

---

## Slide 3 — The Software-to-Hardware Translation Matrix

| C Codebase Component | Processor Concept |
|---|---|
| `CPUState struct` | Architectural State |
| `regs[32] / pc` | Register File & Program Counter |
| `Memory struct` | Main Memory |
| `decoder.c` | Instruction Decoder |
| `isa_registry.c` | Instruction Dispatch |
| Function Pointers | Instruction Control |
| `rv32i_base.c` | ISA Execution Logic |
| `execute_cycle()` | Processor Cycle |

A cheat sheet for the architecture. Every file and struct in this C project has a physical hardware counterpart.

**Detailed image/diagram description:** A two-column boxed mapping chart, headed **C Codebase Component** and **Processor Concept**. Each paired row is joined across the table with a dashed horizontal line. The rows associate C structs, fields, and source files with processor hardware abstractions.

---

## Slide 4 — The Macro Architecture & Instruction Lifecycle

`program.s` (Assembly Source) → Assembler (Generates Machine Code) → Memory (Stores 32-bit Words) → Fetch → Decode → Execute (The Engine) → CPU State (Architectural State Update).

Next PC Update loops back to the engine.

CPU State contains `pc` and `regs[32]`.

The complete lifecycle of a RISC-V instruction inside the simulator.

**Detailed image/diagram description:** A top-down and left-to-right flowchart. Assembly source enters an **Assembler**, whose output enters a **Memory** block containing a grid of words. A horizontal arrow feeds a three-stage engine labelled **Fetch**, **Decode**, **Execute**. Its bottom arrow leads to a CPU State box containing `pc` and `regs[32]`. A green **Next PC Update** feedback arrow loops from the engine output to its input, indicating iterative instruction processing.

---

## Slide 5 — Anatomy of the CPU State

Register file: `regs[32]`; distinguished register: `x0 (zero)`.

**Read Logic**
```c
cpu_read_reg(idx)
if (idx == 0) return 0;
return cpu->regs[idx & 0x1F];
```

**Write Logic**
```c
cpu_write_reg(idx, val)
if (idx == 0) return;
cpu->regs[idx & 0x1F] = val;
```

Minimal but Strict. Centralizing the x0 zero-enforcement means individual instruction logic (like `exec_add`) never needs to manually check if it is writing to the zero register.

**Detailed image/diagram description:** A tall, segmented `regs[32]` register-file illustration sits on the left with its top entry highlighted as `x0 (zero)`. **Read Attempt** and **Write Attempt (val)** arrows travel to a large **The Gatekeeper** enclosure. Inside are separate C read and write logic boxes. Arrows return values or update registers, emphasizing that both paths explicitly protect the architectural zero register.

---

## Slide 6 — Memory & Little-Endian Reconstruction

Memory byte array: `(uint32_t)mem->data[addr] | ...` (assembled with shifts).

| Address | Byte shown | Shift |
|---|---|---|
| `0x103` | `12` | `<< 24` |
| `0x102` | `34` | `<< 16` |
| `0x101` | `56` | `<< 8` |
| `0x100` | `78` | `<< 0` |

Reconstructed 32-bit value: `0x12345678`.

Simulated memory is a simple byte array, but reconstructing 32-bit words requires explicit bitwise logic to simulate little-endian architecture natively and avoid C aliasing issues.

**Detailed image/diagram description:** Four memory cells are stacked in descending address order from `0x103` at top to `0x100` at bottom. They contain hexadecimal bytes `12`, `34`, `56`, and `78`. Arrows route each byte into a shift unit labeled `<<24`, `<<16`, `<<8`, or `<<0`. The four shifted outputs merge into one boxed 32-bit word `0x12345678`. This demonstrates how the lowest-address byte is the least significant byte.

---

## Slide 7 — The ISA Registry: The Engine’s Brain

Physical Key: Opcode (`0x33`), Funct3 (`0`), Funct7 (`0x00`).

ISA Registry Table (Data Structure):

| Opcode | Funct3 | Funct7 | EXEC Function Pointer |
|---|---|---|---|
| `0x33` | `0` | `0x00` | `exec_add` |

Execution Logic: `exec_add`.

The ISA Registry treats instruction definitions as a data table mapped to C function pointers. This elegantly replicates hardware dynamic dispatch and makes adding new ISA extensions trivial by avoiding messy switch statements.

**Detailed image/diagram description:** Three vertically stacked input boxes on the left, labelled Opcode, Funct3, and Funct7, send arrows to a highlighted matching row inside a large ISA Registry Table. The row has fields `0x33`, `0`, `0x00`, and `exec_add`; a thick arrow points from the function-pointer field to an **EXECUTION LOGIC** box labeled `exec_add`. Other empty registry rows suggest additional instruction handlers.

---

## Slide 8 — The 4-Stage Execute Cycle

Four stages: **1. Fetch**, **2. Decode**, **3. Execute**, **4. Update PC**.

Fetch: `memory_read_word(mem, cpu->pc, &raw);`
Decode: `decode_instruction(...)` — decode opcode, `rs1`, `rd`, immediate and other fields.
Execute: `handler->execute(cpu, mem, &instr);` — instruction-specific logic such as `exec_add`.
Update PC: `pc = next_pc;`

The heartbeat of the simulator. `execute_cycle()` mathematically orchestrates the flow of data through the entire system.

**Detailed image/diagram description:** A central four-quadrant circular cycle with thick directional arrows runs clockwise through **Fetch**, **Decode**, **Execute**, and **Update PC**. The Fetch quadrant illustrates a PC querying memory for a raw 32-bit word. Decode illustrates the binary instruction split into fields and the decoder. Execute shows two operands entering an ALU, followed by a function handler. Update PC shows `0x100` advancing to `0x104` or branching/jumping to `0x108`. The quadrants together explain the sequential (not overlapping pipeline) simulator cycle.

---

## Slide 9 — Elegant Control Flow: The next_pc Logic

Default: `next_pc = pc + 4`.

Normal Arithmetic/Logic: Default Fall-Through.

If `beq` is taken: `next_pc = pc + offset`.

Commit: `pc = next_pc`.

Decoupling the current PC from the next PC creates a clean way to model control flow without messy conditionals, preparing the architecture for future pipelining.

**Detailed image/diagram description:** A green branching flowchart starts from the default assignment `next_pc = pc + 4`. The normal arithmetic/logic path descends directly to the final assignment `pc = next_pc`. A side branch labeled **beq taken** enters `next_pc = pc + offset`, then rejoins above the final assignment. This distinguishes ordinary sequential execution from a taken branch without changing the final update mechanism.

---

## Slide 10 — Instruction Deep Dive: Arithmetic & Logic

Simulating Hardware Barrel Shifters:
- 32-bit input vector (`rs2`) with bit positions `31 ... 5 4 ... 0`.
- Discard 27 bits to retain the low five bits.
- `shift_amount = rs2 & 0x1F`.
- Use the masked shift amount in the barrel shifter to generate the shifted value.

Signed vs. Unsigned Semantics:
- `slt` (Signed Less Than): `int32_t (-1) < 1` → True.
- `sltu` (Unsigned Less Than): `uint32_t (4294967295) < 1` → False.

The C native type system and bitwise operators (`int32_t` vs `uint32_t`) are beautifully leveraged as literal hardware mechanisms to natively model complex ISA semantics.

**Detailed image/diagram description:** The slide is divided into two schematic panels. The left panel has a 32-bit `rs2` bus, a marked divide between bits 4 and 5, a crossed-out upper 27-bit portion, and a filtering/masking mesh that sends five bits to a **Barrel Shifter** block. The right panel compares two rows: `slt` using signed `int32_t` semantics, and `sltu` using unsigned `uint32_t` semantics, illustrating opposite results for the same all-ones bit pattern.

---

## Slide 11 — Instruction Deep Dive: Memory & Addresses

Extracted Immediate Value: `imm[11:0]`.

Sign Extend (`lb`) vs Zero Extend (`lbu`):
- Zero Extend (`lbu`): Zero Padding to 32 bits.
- Sign Extend (`lb`): Sign-bit replication to 32 bits.

Base Register (`rs1`, illustrated as `x1`) and extended immediate enter the ALU adder (`+`).

Effective address: `Memory[Base + Offset]`.

Loaded Value (to `rd`).

For memory calculations, the decoder does the heavy lifting of preparing and sign-extending immediate values exactly like a physical hardware decode stage, before it reaches execution.

**Detailed image/diagram description:** On the left an `imm[11:0]` field feeds a two-way conceptual sign/zero extension block. One branch leads to **Zero Padding**, the other to **Sign-bit replication**, both producing 32 bits. The extended immediate enters an ALU marked `[+]` along with the `x1` base-register value, and its sum addresses a tall byte-addressed memory grid (rows 0–11), highlighting **Memory[Base + Offset]**. A downward arrow represents the load result delivered to `rd`.

---

## Slide 12 — The Assembler Toolchain

Pass 1: Build the Map — Assembly Code → Symbol Table (`loop -> 0x0c`).

Pass 2: Resolve and Encode — `address(done) - address(beq)` → binary instruction encoding.

The assembler requires two passes to resolve forward-referencing labels. It relies on the exact same ISA registry as the decoder, establishing a single unified source of truth for the entire toolchain.

**Detailed image/diagram description:** A central **ISA Registry** block connects by arrows to two boxes below it. **Pass 1: Build the Map** contains an assembly-code document icon leading into a small symbol table, with sample label `loop -> 0x0c`. **Pass 2: Resolve and Encode** shows a label-address difference `address(done) - address(beq)` feeding a long binary word. A horizontal arrow from Pass 1 to Pass 2 shows the symbol map being used to resolve references.

---

## Slide 13 — Tracing a Single Instruction

Assembly: `add t0, t1, t2`.
Fields: `op=0x33, f3=0, f7=0x00`.
Encoding: `[ 32-bit Machine Code Binary ]`.
Memory: `[address] = instruction`.
Fetch: Raw word pulled.
Decode: Matches `exec_add()`.
Execute: `R[t0] = R[t1] + R[t2]` → `PC = PC + 4`.

Putting it all together. A single instruction's journey from human-readable text to a permanent change in architectural state.

**Detailed image/diagram description:** A descending seven-step trace displays labels on the left — **Assembly**, **Fields**, **Encoding**, **Memory**, **Fetch**, **Decode**, **Execute** — each aligned with a stylized circle. Colored arrows connect each step to a rectangular explanation box, progressing from assembly mnemonic through decoded fields and machine bits to stored memory, fetched word, chosen handler, and register/PC updates.

---

## Slide 14 — Synthesis: The Three Layers of Understanding

Semantics (The Meaning): `R[t0] = R[t1] + R[t2]`.

Encoding (The Shape): `funct7 | rs2 | rs1 | funct3 | rd | opcode`.

Execution (The Mechanism): Read → ALU → Write.

To master computer architecture, you must never mix these up. Understand an instruction’s meaning, its shape, and its mechanical execution as distinct but seamlessly connected realities.

**Detailed image/diagram description:** Three layered, perspective-drawn plates are stacked vertically. The top plate is **Semantics** and contains the expression `R[t0] = R[t1] + R[t2]`. The middle **Encoding** plate shows a horizontal R-type bit-field strip split into `funct7`, `rs2`, `rs1`, `funct3`, `rd`, and `opcode`. The lower **Execution** plate shows a mini datapath, **Read → ALU → Write**. Dashed guide lines link the plates to indicate they describe the same instruction at different conceptual levels.

---

## Slide 15 — The Future: Evolving the Architecture

Single-Cycle RV32I (Current State) → Pipeline Registers & 5-Stage Execution (IF, ID, EX, MEM, WB) → Data Hazards & Forwarding Logic → Branch Prediction (Static → Gshare) → Custom ISA Extensions (RV32M, Floating Point).

This codebase is a launchpad. Because of its strict modular separation of decoding, dispatch, and execution, introducing pipeline registers and hazards is a natural, seamless progression.

**Detailed image/diagram description:** An upward, left-to-right stair-step roadmap shows five connected blocks, each raised above the previous one. Starting with **Single-Cycle RV32I**, it ascends through **Pipeline Registers & 5-Stage Execution**, **Data Hazards & Forwarding Logic**, **Branch Prediction**, and **Custom ISA Extensions**. Thin arrows connect the blocks, portraying a progression of simulator sophistication.

---

