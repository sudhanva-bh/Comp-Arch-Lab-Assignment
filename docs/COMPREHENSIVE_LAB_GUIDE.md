# Comprehensive Lab Guide

This manual explains the implemented C code in `riscv_sim_base/`. Lab numbers
follow the sheets: Lab 1=W6, Lab 2=W7, Lab 3=W8, Lab 4=W9. Lab 5=W10 is a design
specification only. Use [tutorial.md](tutorial.md) for executable commands.

## 1. The simulation boundary

A simulator state transition models a processor clock. It manipulates a finite
integer register file, byte-addressable RAM and instruction records; it does not
model transistor delays. The original engine completes one supported instruction
per call. The pipeline advances up to five different instructions per call.
Consequently, modeled cycle count and host execution time are distinct metrics.

Correctness is architectural: final integer registers, memory, program output,
termination status and fault address must agree for supported programs under safe
modes. The pipeline fetch PC legitimately differs from the original engine's PC.
Timing correctness is judged within the selected sheet or reference model.

## 2. Lab 1: architectural state and the reference engine

### CPU state and register semantics

`core/cpu_state.h` defines `CPUState`: `regs[32]`, `pc`, `next_pc`, `status`,
`cycle_count`, `fault_addr` and `fault_msg`. `cpu_reset` clears state and chooses
the assembled entry point. `cpu_read_reg` returns zero for x0, and
`cpu_write_reg` discards writes to it. Handlers use these accessors instead of
reimplementing the zero rule. ABI names such as t0=x5 and a7=x17 are aliases,
not extra registers.

Unsigned register storage naturally models 32-bit modular addition/subtraction.
Signed comparisons and arithmetic right shifts cast operands to `int32_t`.
Shift counts are masked to five bits. These choices distinguish signed `slt`
from unsigned `sltu`, and arithmetic `sra` from logical `srl`.

### Memory

`core/memory.c` allocates zero-filled RAM; main defaults to 1,048,576 bytes.
Read/write helpers validate the entire byte/half/word access. Bytes are combined
explicitly in little-endian order: bytes 78 56 34 12 at ascending addresses form
0x12345678. The helpers do not rely on casting a byte pointer to a word pointer,
which avoids host-alignment and aliasing dependencies. This educational model
allows unaligned accesses if all accessed bytes are in bounds; it does not
implement a separate architectural misalignment trap.

### Instruction registry: a shared description of the ISA

`core/isa_registry.h` defines `InstructionHandler` with mnemonic, opcode,
funct3/funct7 match values, format and an execution function pointer. A value of
-1 in a function field means it is not constrained by that registry entry.
`register_rv32i_base` populates the registry before assembly or execution.

The assembler looks up a mnemonic; the decoder looks up encoding fields. Both
consult the same table, so instruction naming and matching share a source of
truth. `isa_find_by_opcode` selects the first encoding match, whereas mnemonic
lookup selects the last matching registration. Avoid ambiguous registrations.

```text
Assembly mnemonic --> registry lookup --> encoder --> memory word
                                                    |
Word --> decoder --> registry handler --> ExecuteFn(cpu, mem, instr)
```

The original executor calls a handler directly. The present pipeline handles
ALU/control/memory operations explicitly using decoded controls; only system
handling invokes the registered execution handler at WB. Thus adding an arbitrary
new registry entry alone does not automatically add its semantics to this
pipeline. W10 needs the extra metadata and execution infrastructure described
later. The registry is extensible, but support across all engines must be checked.

### Two-pass assembler

`assembler/assembler.c` records labels and instruction/directive sizes in pass 1;
pass 2 resolves symbols, parses operands, encodes words and writes memory.
Branch/jump label operands become PC-relative offsets. `li` and `la` expand to
`lui` plus `addi` and always occupy eight bytes, even for a small constant.
Here `la` is an absolute flat-memory address load, not an ELF relocation toolchain.

`.text` and `.data` are accepted section markers, but storage remains one flat
address stream. `.align n` rounds to a multiple of n bytes. Code starts at zero;
entry is `main` if defined. `.word` can deliberately insert an illegal word for
tests. Invalid syntax reports an assembler line number and message.

### Single-cycle execution

`core/executor.c:execute_cycle` fetches from `pc`, decodes, initializes
`next_pc=pc+4`, calls the handler, and updates `pc` if still running. A branch or
jump can overwrite `next_pc`. `cycle_count` increments for an executed handler,
including an exit `ecall`; immediate fetch/decode failure returns before that
increment. This distinction matters when comparing fault timing.

For the supported syscall convention, a7=1 prints signed a0 with no automatic
separator, and a7=10 sets `CPU_ECALL_EXIT`. Other syscall numbers warn and
continue. `main.c` returns zero after execution even when CPU status reports a
fault or cycle limit: execution scripts must inspect status, not just process
exit code. Assembly and invalid-option failures do return a nonzero code.

## 3. Lab 2: synchronous five-stage execution

### Latches define the clock boundary

`pipeline/pipeline_state.h` contains:

| Latch | Producer → consumer | Important contents |
|---|---|---|
| IFID | IF → ID | valid, PC, raw instruction, fetch fault, predicted next PC, accumulated stall metadata |
| IDEX | ID → EX | decoded instruction, handler, controls, captured operands, PC, fault, prediction |
| EXMEM | EX → MEM | ALU result/address, store value, controls, fault address |
| MEMWB | MEM → WB | final writeback value, system argument, controls and deferred fault |

Every stage ignores an invalid latch. Valid=0 represents an empty slot, or
bubble; it is not a decoded `nop`, which is a real instruction that retires.

The key idiom in `pipeline_cycle` is:

```c
Pipeline nx = *p;       /* snapshot the currently latched state */
/* read p->... and write nx.... */
*p = nx;               /* commit one modeled clock edge */
```

Downstream stages cannot read a result produced into a next latch in the same
clock. Otherwise a single C call could carry an instruction through several
stages: a software timing error that effectively moves information backward
through the modeled clock boundary. Architectural register writes in WB are a
deliberate exception: WB runs before ID reads, modeling write-first/read-second
register-file timing. Predictor updates also intentionally precede IF prediction
within the same call, as specified by this implementation.

### Controls produced in ID

`pipeline/control.c:control_decode` classifies the registry's format into
`ControlSignals`: `reg_write`, `wb_src`, `mem_read`, `mem_write`, branch/jump/system
flags and `uses_rs1/uses_rs2`. The source-use flags prevent false dependencies on
bits that happen to occupy register-field positions in U/J/immediate encodings.
Writeback selects no value, ALU result, memory result, or PC+4. Loads/stores use
the ALU to calculate base+signed-immediate addresses.

### Stage responsibilities and ordering

The C implementation evaluates WB, MEM, EX, ID, IF, then commits next state.
This gives older terminal instructions priority over younger side effects.

1. **WB:** process a deferred fault, write a destination, or run an `ecall`.
   Retire only a valid nonfaulting instruction. On exit/fault return immediately,
   before the younger MEM-stage store can write RAM in that call.
2. **MEM:** pass ALU/link results through; perform sized loads/stores; sign-extend
   lb/lh, zero-extend lbu/lhu; capture failures in MEMWB rather than halt early.
3. **EX:** choose forwarded or captured operands; compute ALU result, address,
   branch condition, jump link and target. jalr clears target bit zero.
4. **ID:** reuse `decode_instruction`, generate controls, consult hazard logic,
   and capture register operands if advancing.
5. **IF:** fetch unless stalled or spending a reference redirect clock. A failed
   fetch becomes `PF_FETCH` metadata. Advance the fetch pointer or predicted PC.

### Precise faults and control flow

Fault kinds are `PF_NONE`, `PF_ILLEGAL`, `PF_FETCH`, `PF_MEM`. ID cannot expose an
illegal word immediately: it might be younger than an exit syscall or a branch
that will squash it. Metadata travels in order to WB so older instructions finish
and younger stores/register writes cannot commit after a visible fault.

A branch redirect has priority over a younger decode fault or data hazard. In
sheet mode EX fetches the target in the same call, losing one younger ID slot.
Reference mode invalidates both younger slots and starts the target next clock,
losing two cycles. A flush event counter is not itself a cycle-penalty counter.

## 4. Lab 3: hazard detection and forwarding

### Producer qualification

`pipeline/hazard.c:produces` requires a valid latch, `reg_write`, nonzero rd and
rd equal to the requested source. Each source is checked only if its control flag
says it is used. This excludes bubbles, x0 destinations, stores and branches as
register producers. A branch can still be a **consumer** of prior results.

### Interlock policies

Sheet stalling checks ID/EX, EX/MEM and MEM/WB. A back-to-back consumer waits three
clocks; distance-two and distance-three consumers wait two and one respectively.
The final WB wait is conservative because this implementation already permits
WB-to-ID reads. It is retained to reproduce the handout's tables.

Reference stalling checks only ID/EX and EX/MEM, using the WB-first register-file
ordering to avoid that last stall. With forwarding, normal ALU dependences do
not stall; only the relevant ID/EX load-use dependence does.

A stall is always three coordinated actions: preserve IFID, freeze fetch PC, and
emit an invalid IDEX. Simply holding a consumer while fetch overwrites IFID loses
an instruction. The verdict distinguishes ordinary stalls from load-use stalls.

### The forwarding multiplexers

For each used, nonzero source, `forward_one` selects:

```text
EX/MEM ALU result (youngest matching non-load producer) ---+
MEM/WB final value (next matching producer) --------------+--> EX operand
ID/EX captured register value (fallback) -----------------+
```

EX/MEM wins because two in-flight writes to the same register must expose the
newer value. Faulting producers are excluded. This explicit condition is crucial:

```c
!p->ex_mem.ctrl.mem_read
```

For a load, `ex_mem.alu_result` is its **address**, not the data requested from
that address. Forwarding it would quietly turn a load dependency into a wrong
calculation. The interlock delays the immediate consumer until MEM/WB holds the
actual loaded value.

Both selected operands feed arithmetic, store data, branch comparisons and the
jalr base. Bypassing only arithmetic leaves memory and control-flow bugs. Forward
counters count each operand independently; one instruction can add two.

### Why a load still needs one stall

A producer load in MEM obtains data at the end of its clock, while an adjacent
consumer would need it at the beginning of EX in that same clock. The old-latch
model cannot access that future value. Stall once; then load data exists in
MEMWB when the consumer reaches EX. A load used as store data also costs one
stall here because there is no dedicated MEM-to-MEM bypass.

### System operands

Reference timing treats `ecall` as consuming a7/x17 and a0/x10: ID sets both source
numbers and use flags, EX can forward them, MEMWB carries the arguments, and WB
executes with a scratch CPUState containing those captured values. Sheet timing
instead reads committed architectural registers at WB; it does not interlock on
implicit syscall operands. This accounts for differences in stall/forward totals.

## 5. Lab 4: prediction at fetch, verification at EX

### Interface and state ownership

`branch_predictor.h` exposes `reset`, const `predict`, and `update` callbacks.
`BPState` is owned by each `Pipeline`, not by static mutable globals. A predictor
sees only branch PC and target; actual register operands are unavailable to it.
`pipeline_set_predictor` resets the chosen table and enables reference timing.

IF decodes enough information, using the existing decoder, to determine whether
a word is a conditional branch or direct jal. It follows a jal target immediately
and asks the selected predictor for conditional direction. `pred_next` travels
with IFID/IDEX and stays unchanged while held by a stall. Only resolved branches
in EX train the table; a younger branch squashed before EX cannot train it.
EX updates `nx.bp` before IF consults that next predictor state in the same clock.

### Static direction rules

Always-not-taken chooses PC+4. BTFN predicts taken iff target<PC: it exploits loop
layout without storing history. Equal PC/target is not backward under this rule.
Both have no learning state; their update callbacks do nothing.

### One-bit and two-bit machines

One-bit stores the last actual outcome, initially 0. Every loop exit changes its
next prediction, so entering the loop again can cause another miss.

Two-bit starts at weakly-not-taken (1) and uses hysteresis:

| State | Meaning | Prediction | On N | On T |
|---:|---|---|---:|---:|
| 0 | Strongly not taken | N | 0 | 1 |
| 1 | Weakly not taken | N | 0 | 2 |
| 2 | Weakly taken | T | 1 | 3 |
| 3 | Strongly taken | T | 2 | 3 |

Updates saturate, never wrap. A loop exit from state 3 weakens to 2 without
reversing the prediction, which often helps repeated/nested loops. Alternating
T/N patterns can keep flipping weak states and defeat both simple predictors.

The index is `(pc >> 2) & 63`, i.e. PC bits 7:2. Dropping the low two alignment
bits gives 64 entries. Branch PCs 256 bytes apart select the same untagged entry
and train each other. b2_adversarial intentionally exercises this interference.
More entries, tags or history-dependent indexing would change the design and are
not part of W9.

### Resolution, recovery and counting

EX computes the actual conditional next PC and compares it with `pred_next`.
A mismatch increments `mispredicts`, squashes younger work and redirects. Correct
prediction costs zero control cycles. `jal` still writes PC+4, but its IF target
selection normally eliminates redirects. `jalr` has no target predictor or
return-address stack and always incurs an EX redirect, including fall-through
coincidence. Accuracy is conditional prediction accuracy only.

Counters: `branches` counts resolved conditionals; `jal` and `jalr` count resolved
jumps; `flushes` is reported as W9 `redirects`; `squashed=2*redirects` is the
reference's nominal discarded-slot count. No-branch runs report accuracy 0.0
rather than divide by zero. The pipeline summary separately prints stalls,
load-use stalls and EX/MEM vs MEM/WB operand forwards.

## 6. Timing equations and terminal drain

For normal successful runs without terminal speculative overlap:

$$\mathrm{CPI}=\frac{\mathrm{cycles}}{\mathrm{retired}}$$

$$C_{sheet}=I+S+F+4,\qquad C_{reference}=I+S+2F+4$$

W9 further satisfies:

$$F=M+J_{indirect},\qquad A=100\frac{B-M}{B}\quad(B>0).$$

Four is the five-stage fill/drain overhead relative to retirement count. Exit
itself retires. Fetching beyond the last meaningful word does not retire extra
instructions if exit reaches WB first.

A younger ID-held cycle may occur while an older exit is draining. It is a real
stall event but does not delay exit. Latches carry `stalled_cycles`; terminal WB
sums the younger unretired contributions into `discarded_stalls`. For the tested
terminal-stall cases, subtract that quantity from the simple equation. It is 1
in s3_control's stalling configurations and b3_applied's W9 stalling mode. It is
not subtracted from the raw physical `stalls` counter. Arbitrary younger redirects
could also overlap termination, so the simple identity assumes their absence.
Do not apply normal-exit identities mechanically to faulted or cycle-limited runs.

A perfect direction oracle could have zero conditional misses. Loop warm-up and
exit misses describe limitations of these particular predictors, not a universal
mathematical floor. Indirect redirects remain outside the direction predictor's
scope: b3_applied has 191, costing 382 control cycles even with perfect direction.

## 7. Verification architecture

`make test` first builds `pipeline_checks` and `predictor_checks`. The Python
harness reports 747 checks of goldens, values, output, timing, limits, traces and
CLI behavior. `check_state.c` independently compares all integer registers and
every memory byte with a separately assembled single-cycle run. It covers both
safe hazards and all predictors; final fetch PC is excluded. `check_predictor.c`
asserts transition endpoints, saturation, reset, immutability of prediction,
aliasing, lookup and instance isolation. Eight assembly regressions target faults,
exit suppression, wrong-path control flow, x0 and loop-exit recovery.

Independent supplied goldens guard against shared-oracle bugs. The suite matches
all implemented sample golden expectations, including intentionally faulty
no-hazard results; 100% tested safe-mode oracle agreement is not an exhaustive ISA
proof. Original t1–t4 W9 and sample3/sample4 W7/W8 sources were absent, so their
specific sheet rows are not claimed as measured. The supplied 12 executable
samples are used instead. FP fixtures remain excluded. Sanitizers passed; native
MSVC and RARS were not run in this environment.

## 8. Lab 5 specification: infrastructure before RV32F handlers

The W10 sheet assumes infrastructure that is absent from this W9 code. Its
one-new-ISA-file assignment only applies after that starter infrastructure exists.
No `fp_helpers.c`, `zicsr.c`, `rv32f.c`, FP register file or FP pipeline is currently
implemented. A staged plan is:

1. Extend CPUState with raw-bit `fregs[32]` and `fcsr`; f0 is writable. Add register
   accessors, reset/display and sticky flag helpers.
2. Extend the registry with operand-file metadata, rounding-mode fields, fixed-rs2
   matching and formats for two-source, three-source and CSR operations. Decode
   `rs3` and CSR numbers; preserve legacy registrations.
3. Teach the assembler f0–f31/ABI names, `.float`, FP pseudo-ops, optional static
   rounding modes and CSR forms.
4. Map integer and FP sources to unified dependency identifiers: integer x1–x31
   use 1–31, FP f0–f31 use 32–63, while x0 becomes REG_NONE for dependency checks.
   Do not suppress writes to f0 by reusing the integer zero rule. Add `src3/fwd3`
   and a captured/forwarded third operand for fused multiply-adds.
5. Extend pipeline execution so registered FP handlers operate with latched or
   forwarded inputs and produce the correct destination file at WB. Make flw/fsw
   use integer address bases and FP data sources/destinations.
6. Implement and independently verify numerical helpers and the six CSR forms:
   csrrw/csrrs/csrrc and their immediate variants. Read/write fflags, frm and fcsr.
7. Implement the 26 instructions listed below and register them. Re-run integer
   regressions before validating FP values, flags and timing.

| Group | Instructions |
|---|---|
| Raw bits / classification (8) | flw, fsw, fsgnj.s, fsgnjn.s, fsgnjx.s, fmv.x.w, fmv.w.x, fclass.s |
| Arithmetic / fused (9) | fadd.s, fsub.s, fmul.s, fdiv.s, fsqrt.s, fmadd.s, fmsub.s, fnmsub.s, fnmadd.s |
| Compare / min-max / convert (9) | feq.s, flt.s, fle.s, fmin.s, fmax.s, fcvt.w.s, fcvt.wu.s, fcvt.s.w, fcvt.s.wu |

Binary32 is sign:1, exponent:8, fraction:23. Preserve raw bits with memcpy-based
conversion helpers, not pointer punning. Canonical arithmetic NaN is 0x7fc00000;
bit-moving/sign operations preserve payloads. Distinguish quiet and signalling
NaNs, signed zeros, exact subnormals and inexact underflow. Float-to-integer
conversion must range-check and saturate before casting to avoid undefined C
behavior. Fused operations round once, not after separate multiplication/addition.

`fcsr` holds sticky flags NX, UF, OF, DZ, NV in bits 0–4 and frm in bits 7:5.
Support RNE, RTZ, RDN, RUP, RMM, plus dynamic mode using frm. Invalid rounding
encodings must fault before writing flags or a destination. RMM has no direct
host fenv equivalent; numerical helpers need careful tie handling. Host arithmetic
alone is insufficient to establish RISC-V NaN/flag/rounding correctness.

The sheet plans fcsr updates at EX in order, with CSR accesses in the same stage;
that can avoid a separate fcsr bypass. The implementation must still guarantee
that an older fault or terminal instruction cannot leave a younger irreversible
fcsr update visible. This is an invariant to establish, not a property supplied
by today's integer-only pipeline. The W10 sheet also assumes an RV32M module;
that module is absent and should be supplied or implemented if required by the
chosen future FP programs. No M-extension implementation is claimed here.
