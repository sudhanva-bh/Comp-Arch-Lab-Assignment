# Lab Sheet 5: Floating-Point Arithmetic (RV32F)

## 1. Objectives

The previous three labs each added a unit to the pipeline. This one adds nothing to the pipeline at all. Floating point arrives entirely through the ISA registry: you write handler functions, register them with their encodings, and the decoder, the assembler, the hazard unit and the forwarding muxes pick them up without knowing they exist.

That is the architectural claim, and it is worth being skeptical about before you start — floating point brings a second register file, a status register and a third source operand, none of which the base ISA has. Section 2 says exactly which parts of that were paid for in advance and which you get for free.

By the end you will:

1. Read the single-precision format well enough to write instructions that manipulate it as bits: loads, stores, moves, sign injection, classification.
2. Implement the arithmetic fadd.s through fsqrt.s and the four fused multiply-adds with correct rounding under all five modes.
3. Implement comparisons, min/max and integer conversions, where the RISC-V rules deliberately differ from what C would do, and where every NaN is a trap for the unwary.
4. Accrue the five IEEE exception flags in fcsr and read them back with the CSR instructions.
5. Measure FP code on your pipeline, and show that FP values are forwarded and stalled by exactly the machinery you built in Lab 3.

## 2. What You Start From

Your pipeline with hazard handling and branch prediction, from the previous labs. Check it before writing any floating point:

```bash
cd lab5
make
./rvsim tests/t4_fp_program.s --batch --single-cycle                 # oracle: 133 cycles
./rvsim tests/t4_fp_program.s --batch                                # 159 cycles, prints
```

`12000014141000`

The folder also contains the M extension (mul, div, rem and family) as given code. If you have not met it yet. Read it first, as a worked example of exactly what you are about to do: eight instructions added through the registry, with no change to the decoder, the assembler or the pipeline.

### What is given, and what you write

This lab needs more given infrastructure than the previous ones, because the base ISA has no concept of a second register file. All of that is in place:

```text
File                                            Given or yours
core/cpu_state.*                                given: fregs[32], fcsr,
                                                cpu_read_freg/cpu_write_freg, cpu_raise_fflags,
                                                cpu_frm

core/isa_registry.*                             given: formats FMT_R2/FMT_R4/FMT_CSR, and
                                                register_instruction_ex(handler, flags,
                                                rs2_sel)

core/decoder.c                                  given: decodes rs3, CSR numbers, and fixed-rs2
                                                encodings
assembler/assembler.c                           given: f0-f31 and ABI names, the optional
                                                rounding-mode operand, .float

pipeline/control.c, pipeline/*                  given: unified register numbers, the third source src3
                                                and its forwarding select fwd3
isa/fp_helpers.c                                given: f_of/bits_of, resolve_rm, host_mode,
                                                fflags_of, d2f, fp_arith

isa/zicsr.c                                     given: the six CSR instructions
isa/rv32f.c                                     yours — all four tasks
main.c                                          one line you add: register_rv32f();
```

The helpers in isa/fp_helpers.c are given because they are numerical plumbing, not architecture: setting the host rounding mode through `<fenv.h>`, translating the host's exception flags, and the one genuinely nasty case where round-to-nearest-max-magnitude has no host equivalent. Section 6 explains what each does but you do not have to write them.

### The twenty-six instructions you write

All of them go in isa/rv32f.c: eight that only move bits (Task 1), nine arithmetic (Task 2), and nine where the RISC-V rules differ from C (Task 3). Task 4 adds no new instructions, only the flag behaviour of the ones listed here. In the operation column f[] and x[] are the two register files, S is the sign mask 0x80000000, and round() means rounding in the mode the instruction resolves to. The format column gives the registry format, with the fixed rs2 field value where it is not 0.

```text
Instruction   What it does                      Format          Operation
Task 1: bits only. No rounding mode, no exception flags, no NaN cases.
flw           Loads 32 bits from memory         FMT_I_LOAD      f[rd] = mem[x[rs1] + imm]
              into an f register. The address
              base is an x register.
fsw           Stores the raw 32 bits of an f    FMT_S           mem[x[rs1] + imm] = f[rs2]
              register to memory.

Instruction   What it does                       Format        Operation
fsgnj.s       Magnitude of fs1 with the sign     FMT_R         f[rd] = (f[rs1] & ~S) | (f[rs2] & S)
              of fs2. The assembler builds
              fmv.s from it.
fsgnjn.s      Magnitude of fs1 with the sign     FMT_R         f[rd] = (f[rs1] & ~S) | (~f[rs2] & S)
              of fs2 inverted. Gives fneg.s.
fsgnjx.s      Magnitude of fs1 with the two      FMT_R         f[rd] = f[rs1] ^ (f[rs2] & S)
              signs XOR-ed. Gives fabs.s.
fmv.x.w       Moves the raw bits of an f         FMT_R2        x[rd] = f[rs1]
              register into an x register.
              Reinterprets, does not convert.
fmv.w.x       Moves the raw bits of an x         FMT_R2        f[rd] = x[rs1]
              register into an f register.
fclass.s      Writes a one-hot code naming       FMT_R2        x[rd] = 1 << class(f[rs1])
              which of the ten classes fs1
              falls in.
Task 2: arithmetic. Every one carries a rounding mode (IF_HAS_RM) and can raise NV, DZ, OF, UF or NX.
fadd.s        Adds two floats and rounds         FMT_R         f[rd] = round(f[rs1] + f[rs2])
              once.
fsub.s        Subtracts fs2 from fs1 and         FMT_R         f[rd] = round(f[rs1] - f[rs2])
              rounds once.
fmul.s        Multiplies and rounds once.        FMT_R         f[rd] = round(f[rs1]*f[rs2])
fdiv.s        Divides. A finite non-zero value   FMT_R         f[rd] = round(f[rs1] / f[rs2])
              over zero raises DZ.
fsqrt.s       Square root of one operand. A      FMT_R2        f[rd] = round(sqrt(f[rs1]))
              negative operand gives the
              canonical NaN and NV.
fmadd.s       Multiply then add, with a          FMT_R4        f[rd] = round(f[rs1]*f[rs2] + f[rs3])
              single rounding at the end.
fmsub.s       Multiply then subtract the         FMT_R4        f[rd] = round(f[rs1]*f[rs2] - f[rs3])
              third operand, one rounding.
fnmsub.s      Negated product plus the third     FMT_R4        f[rd] = round(-(f[rs1]*f[rs2]) +
              operand. Read the operation,                     f[rs3])
              not the name: it adds.
fnmadd.s      Negated product minus the          FMT_R4        f[rd] = round(-(f[rs1]*f[rs2]) -
              third operand. Read the                          f[rs3])
              operation, not the name: it
              subtracts.
Task 3: comparisons, min and max, and conversions. NaN and the two zeros drive every rule here.
feq.s         Quiet equality test into an x      FMT_R         x[rd] = (f[rs1] == f[rs2]) ? 1 : 0
              register. NV only when an
              operand is a signalling NaN.
flt.s         Signalling less-than. Any NaN      FMT_R         x[rd] = (f[rs1] < f[rs2]) ? 1 : 0
              gives 0 and raises NV.

Instruction   What it does                      Format        Operation
fle.s         Signalling less-than-or-equal.    FMT_R         x[rd] = (f[rs1] <= f[rs2]) ? 1 : 0
              Any NaN gives 0 and raises NV.
fmin.s        Smaller operand. A NaN never      FMT_R         f[rd] = min(f[rs1], f[rs2])
              wins, and fmin.s(-0, +0) is -0.
fmax.s        Larger operand. A NaN never       FMT_R         f[rd] = max(f[rs1], f[rs2])
              wins, and fmax.s(-0, +0) is +0.
fcvt.w.s      Float to signed 32-bit integer,   FMT_R2        x[rd] = int32(round(f[rs1]))
              rounded per rm, saturating on
              NaN or out of range.
fcvt.wu.s     Float to unsigned 32-bit          FMT_R2, rs2   x[rd] = uint32(round(f[rs1]))
              integer, same saturation rules.   = 1

fcvt.s.w      Signed 32-bit integer to float,   FMT_R2        f[rd] = round(float(int32(x[rs1])))
              rounded per rm. Raises NX
              above 24 significant bits.
fcvt.s.wu     Unsigned 32-bit integer to        FMT_R2, rs2   f[rd] = round(float(uint32(x[rs1])))
              float, rounded per rm.            = 1
```

fmv.s, fneg.s and fabs.s are absent because they are not instructions: the assembler expands them into the three sign injections with the same register named twice. The six CSR instructions that read and write fcsr are given in isa/zicsr.c.

### Why the pipeline needs no changes

Three decisions made earlier in the labs pay off here, and the report asks you to explain them:

- Operand metadata lives beside the registry, not in the instruction. register_instruction_ex takes flags (IF_RD_FP, IF_RS1_FP, IF_HAS_RM, IF_RS2_FIXED) saying which operands are f registers. Every existing six-field register_instruction() call in the base ISA still compiles untouched.
- The pipeline uses unified register numbers. control_from_instr() maps x1-x31 to 1-31 and f0-f31 to 32-63, so src1/src2/src3/dst can name either file and your hazard unit compares them without caring which. x0 is REG_NONE; f0 is an ordinary register.
- flw and fsw are just loads and stores. They go through the MEM stage like lw and sw; only the destination file differs.

### Rules of engagement

- core/, assembler/, ui/ and pipeline/ stay untouched. Read them freely — especially control.c, to see how the flags become register numbers.
- You write isa/rv32f.c and add one line to main.c. Nothing else.
- --single-cycle is still the oracle, and it must agree with the pipeline on every FP program, bit by bit, including fcsr.
- Build warning-free under -Wall -Wextra. The Makefile already passes -frounding-math and links -lm; both matter.

## 3. Background: Thirty-Two Bits, Read Three Ways

A single-precision value is one 32-bit word, split three ways:

```text
  31 30                 23 22                                        0
+---+--------------------+-------------------------------------------+
| S |       exponent      |                fraction                  |
+---+--------------------+-------------------------------------------+
   1          8 bits                       23 bits

   exponent 1..254   normal    value = (-1)^S x 1.fraction x 2^(exp-127)
   exponent 0        zero/sub  fraction 0 -> +-0; else subnormal, 0.fraction x
2^-126
   exponent 255      inf/NaN   fraction 0 -> +-inf; else NaN
```

The three facts that generate almost every rule in this lab:

- There are two zeros. +0 is 0x00000000, -0 is 0x80000000. They compare equal but are not the same bits, and fmin.s(-0, +0) must return -0.
- There are many NaNs. Any exponent of 255 with a non-zero fraction. If fraction bit 22 is set it is quiet; if clear it is signalling and must raise an exception when used in arithmetic. RISC-V requires every NaN produced by arithmetic to be the single canonical NaN 0x7fc00000.
- A NaN is not equal to itself. feq.s(x, x) is 0 when x is NaN, which is why conversions and comparisons need explicit NaN tests rather than ordinary C comparisons.

### The register file and fcsr

The f registers are a second file of 32 words, completely separate from x. There is no f0 == 0 rule: f0 is an ordinary register. Registers hold raw bit patterns, which is why cpu_read_freg returns uint32_t; a value becomes a C float only for the duration of one operation, and only via memcpy, never a pointer cast.

Alongside them sits one status register, fcsr:

```text
bit            7:5                  4          3                    2            1               0

field          frm                  NV         DZ                   OF           UF              NX

meaning        rounding mode        invalid    divide by zero       overflow     underflow       inexact
```

The five flags are sticky: an instruction sets the ones it raises and never clears any. Software clears them explicitly through the CSR instructions. --batch decodes the register for you, so a run ends with a line like fcsr =0x00000001 (frm=0 flags: NX).

### The five rounding modes

```text
rm           Name          Rule

0            rne           to nearest, ties to even (the default)

1            rtz           toward zero

2            rdn           down, toward -inf

rm          Name          Rule

3           rup           up, toward +inf

4           rmm           to nearest, ties away from zero

7           dyn           use fcsr.frm — what you get when the operand is omitted
```

An arithmetic instruction carries its mode in the funct3 field, so fadd.s fs0, fs1, fs2, rtz and fadd.s fs0, fs1, fs2 are different encodings of the same operation. Modes 5 and 6 do not exist, and dyn with an frm of 5-7 is equally invalid: both must raise ILLEGAL_INSTRUCTION rather than guessing.

The modes only matter when a result is inexact, which is most of the time: 2.5 converts to the integers 2, 2, 2, 3, 3 under the five modes respectively, and -2.5 to -2, -2, -3, -2, -3. Those ten values are in the test suite, and getting them right means your rounding is plumbed through correctly rather than hard-coded to the host default.

## 4. Worked Example: One fadd.s, End to End

Before writing thirty instructions, follow one all the way through. Everything you add has this shape.

1. A handler. An ordinary ExecuteFn, the same signature the base ISA has used since Lab 0:
```c
static void exec_fadd(CPUState *c, Memory *m, DecodedInstr *i) { (void)m;
    do_arith(c, i, AR_ADD, FREG(i->rs1), FREG(i->rs2), 0);
}
```

2. A registration, giving the encoding and the operand metadata:
```c
#define FFF (IF_RD_FP | IF_RS1_FP | IF_RS2_FP)   /* fd, fs1, fs2 */

reg("fadd.s", OP_FP, -1, 0x00, FMT_R, exec_fadd, FFF | IF_HAS_RM, 0);
/*     ^        ^     ^    ^      ^        ^          ^
       mnemonic opcode| funct7 format    handler    operand flags
                   funct3 = -1: don't care, it holds the rounding mode */
```

That one line buys four things at once, and this is the part worth understanding:

```text
Because the registry knows...                  ...this works with no further code

the mnemonic                                   the assembler accepts fadd.s fs0, fs1, fs2

opcode + funct7, funct3 don't-care             the decoder matches the instruction, whatever rounding
                                               mode it carries
IF_HAS_RM                                      funct3 is parsed as a rounding mode, not matched
                                               against
FFF                                            control_from_instr() numbers rd/rs1/rs2 in the f
                                               file (32-63), so the hazard unit sees an FP dependence
```

3. The pipeline runs it, unchanged. ID reads fs1 and fs2 out of the f file into the ID/EX latch. EX calls
your handler on a scratch CPUState holding the latched (or forwarded) operands. WB writes fd. Nobody along that path knows what a float is.

4. Dependences are handled by the unit you already built. This is the claim to verify by hand:
```asm
     fadd.s fs0, fs1, fs2           # produces fs0
     fmul.s fs3, fs0, fs0           # consumes fs0 one instruction later
```

The second instruction's src1 and src2 are both the unified number for fs0, and the first instruction's dst is the same number. Your Lab 3 forwarding classifier therefore returns FWD_EX_MEM for both operands, exactly as it would for add/mul on x registers. Likewise:

```asm
    flw    fs0, 0(a0)             # a load whose destination is an f register
    fmul.s fs1, fs0, fs0          # LOAD-USE: one stall, then FWD_MEM_WB
```

t1_fp_arith.s contains exactly this pair, and it is the program's single stall: stalls = 1 under --hazard forward, against stalls = 16 under --hazard stall. Run both and check.

### The one thing that is genuinely new

A fused multiply-add reads three registers:

```asm
     fmadd.s fs0, fs1, fs2, fs3            # fs0 = fs1 * fs2 + fs3, rounded once
```

That is why Control has a third source src3 and HazardDecision a third select fwd3. They were added for this lab and are given; your job is to register FMT_R4 instructions so that src3 gets filled in. If you forget IF_RS3_FP on a fused multiply-add, the dependence on fs3 is invisible to the hazard unit and the instruction silently reads a stale third operand, which is a wrong answer, not a wrong cycle count.

## 5. Task 1 — The Instructions That Never Touch a Float

File you write: isa/rv32f.c

Start here, because none of these instructions does arithmetic. They move and rearrange bits, so they raise no exception flags, consult no rounding mode, and have no NaN special cases and they let you get the registration and the pipeline wiring working before any numerics are in the way.

### Loads and stores

flw and fsw are lw and sw with a different destination file. The address arithmetic still uses an x register:

```c
static void exec_flw(CPUState *c, Memory *m, DecodedInstr *i) {
    uint32_t addr = cpu_read_reg(c, i->rs1) + (uint32_t)i->imm;
    uint32_t v;
    if (memory_read_word(m, addr, &v) != MEM_OK) { fp_mem_fault(c, addr, "flw");
return; }
    cpu_write_freg(c, i->rd, v);
}
```

Note the asymmetric flags at registration: flw is IF_RD_FP (destination in the f file, address source in x), and fsw is IF_RS2_FP (the stored data is an f register, the address base is not).

```c
reg("flw", OP_LOAD_FP, 0x2, -1, FMT_I_LOAD, exec_flw, IF_RD_FP, 0);
reg("fsw", OP_STORE_FP, 0x2, -1, FMT_S,              exec_fsw, IF_RS2_FP, 0);
```

Get these two wrong and t4_fp_program.s will stall in the wrong places even when its answers are right, which is the clearest possible demonstration that operand metadata is what the hazard unit runs on.

### Sign injection

Three instructions that take the magnitude from one operand and the sign from another, with the sign optionally negated or XOR-ed. One line each:

```c
#define SIGN_BIT 0x80000000u

/* fsgnj.s: magnitude of fs1, sign of fs2 */
cpu_write_freg(c, i->rd, (FREG(i->rs1) & ~SIGN_BIT) | (FREG(i->rs2) & SIGN_BIT));
/* fsgnjn.s: magnitude of fs1, sign of fs2 negated */
cpu_write_freg(c, i->rd, (FREG(i->rs1) & ~SIGN_BIT) | (~FREG(i->rs2) & SIGN_BIT));
/* fsgnjx.s: magnitude of fs1, signs XOR-ed */
cpu_write_freg(c, i->rd, FREG(i->rs1) ^ (FREG(i->rs2) & SIGN_BIT));
```

These are how the assembler implements fmv.s fd, fs (fsgnj.s fd, fs, fs), fneg.s (fsgnjn.s fd, fs, fs) and fabs.s (fsgnjx.s fd, fs, fs) so all three pseudo-ops come free once these work. Because they are bit operations, fneg.s of a signalling NaN gives 0xffa00000 and raises nothing: the payload survives. That is in the test suite, and it is correct.

### Moves between the files

```c
/* fmv.x.w rd, fs1 : raw bits of an f register into an x register */
cpu_write_reg(c, i->rd, FREG(i->rs1));
/* fmv.w.x fd, rs1 : and back */
cpu_write_freg(c, i->rd, cpu_read_reg(c, i->rs1));
```

No conversion, no rounding, these reinterpret, they do not convert. (fcvt.s.w is the one that converts; that is Task 3.) Both are FMT_R2 with IF_RS2_FIXED, because the rs2 field is part of the opcode rather than a register.

### Classify

fclass.s rd, fs1 writes a one-hot result: exactly one of ten bits, saying what kind of value fs1 is.

```text
bit         0       1          2            3        4        5         6        7       8        9

meaning -inf        -norm      -subnorm -0           +0       +subnor   +norm    +inf    sNaN     qNaN
                    al         al                             mal       al
```

This is the exercise that forces you to decode the format properly, and it is worth writing before the arithmetic because every later task reuses the same three tests:

```c
int         neg = (b & SIGN_BIT) != 0;
uint32_t exp = (b >> 23) & 0xFF;
uint32_t frac = b & 0x007FFFFFu;
/* exp == 0xFF: frac ? NaN (quiet if bit 22 set) : inf
   exp == 0       : frac ? subnormal : zero
   otherwise : normal */
```

Checkpoint. With Task 1 done, t3_special_values.s will not finish, it needs arithmetic but the fclass and sign-injection registers in its dump must already be right, and the FP parts of t1_fp_arith.s that only load, store and move must match --single-cycle.

## 6. Task 2 — Arithmetic and Rounding

Same file: isa/rv32f.c

Nine instructions: fadd.s, fsub.s, fmul.s, fdiv.s, fsqrt.s, and the four fused multiply-adds. They all funnel into one given helper:

```c
typedef enum { AR_ADD, AR_SUB, AR_MUL, AR_DIV, AR_SQRT, AR_FMA } ArithOp;

/* Computes op(A, B, C) on bit patterns in rounding mode rm. Returns the
  * result bits and ORs the exception flags into *flags. */
uint32_t fp_arith(ArithOp op, uint32_t A, uint32_t B, uint32_t C,
                   int rm, uint32_t *flags);
```

So each handler is three lines, and the whole of Task 2 is a short file:

```c
static void do_arith(CPUState *c, DecodedInstr *i, ArithOp op,
                        uint32_t A, uint32_t B, uint32_t C) {
    int rm = resolve_rm(c, i);                  /* given: funct3, or fcsr.frm for dyn */
    if (rm < 0) return;                         /* illegal rounding mode: already faulted */
    uint32_t flags = 0;
    uint32_t r = fp_arith(op, A, B, C, rm, &flags);
    cpu_raise_fflags(c, flags);                 /* sticky OR into fcsr */
    cpu_write_freg(c, i->rd, r);
}

static void exec_fadd(CPUState *c, Memory *m, DecodedInstr *i) { (void)m;
    do_arith(c, i, AR_ADD, FREG(i->rs1), FREG(i->rs2), 0);
}
```

The order inside do_arith is the thing to get right: resolve the mode first and bail out if it is invalid, because an illegal instruction must not have written a register or a flag.

### What the given helpers do, and why they exist

Read them before you use them; the report asks about two of them.

```text
Helper                        What it handles

f_of / bits_of                uint32_t ↔ float through memcpy. A pointer cast here is a strict-aliasing
                              bug, and compilers do miscompile it
resolve_rm                    the instruction's funct3, or fcsr.frm when it is dyn; raises
                              ILLEGAL_INSTRUCTION for modes 5, 6, and dyn with a bad frm

host_mode                     RISC-V mode → the <fenv.h> constant for fesetround
fflags_of                     the host's fetestexcept bits → NV DZ OF UF NX
d2f                           rounds an exact double to float in a given mode, including the RMM tie
                              fix-up
fp_arith                      runs the operation on the host FPU with rounding set, collects flags, and
                              canonicalises any NaN result to 0x7fc00000
```

Two of those deserve a sentence each, because they are the places where "just use the host FPU" is not enough:

The canonical NaN. RISC-V requires every NaN produced by arithmetic to be exactly 0x7fc00000. An x86 host computing 0.0f/0.0f produces 0xffc00000, the sign bit is set. So fp_arith ends with isnan(r) ? CANONICAL_NAN : bits_of(r). Skip that and you get a plausible-looking NaN that fails the golden files.

Round to nearest, ties away (RMM). `<fenv.h>` has no such mode, so it cannot simply be set. It differs from rne only when the exact result is exactly halfway between two floats, so d2f computes the operation in double, and if that is exact and lands on a tie, rounds away from zero by hand. Quotients and square roots are never exact ties, so this only ever fires for add, subtract, multiply and FMA. The test case is 1 + 2^-24: rne gives 1.0, rmm gives 1.0000001.

### The four fused multiply-adds

One multiply and one add, with a single rounding at the end which is why fmaf must be used rather than a*b+c. The four differ only in which signs are flipped, and sign flipping is a bit operation on the operand before it reaches the FPU:

```text
Instruction      Computes                       Implementation
fmadd.s          fs1 * fs2 + fs3                AR_FMA(A, B, C)

fmsub.s          fs1 * fs2 - fs3                AR_FMA(A, B, C ^ SIGN_BIT)

fnmsub.s         -(fs1 * fs2) + fs3             AR_FMA(A ^ SIGN_BIT, B, C)

fnmadd.s         -(fs1 * fs2) - fs3             AR_FMA(A ^ SIGN_BIT, B, C ^ SIGN_BIT)
```

The two names are counter-intuitive on purpose: fnmsub adds and fnmadd subtracts. Check them against the table rather than against the mnemonic.

These are the FMT_R4 instructions, and the only ones with a third source. Register them with IF_RS3_FP so src3 reaches the hazard unit (Section 4), and with funct3 and funct7 both don't-care: funct3 holds the rounding mode and funct7 holds rs3 plus the format field.

Checkpoint. ./rvsim tests/t1_fp_arith.s --batch must now complete with cycles = 41, retired = 36, stalls = 1, forwards = 11 and fcsr = 0x00000001 (frm=0 flags: NX), matching --single-cycle on every register.

## 7. Task 3 — Where RISC-V Disagrees With C

Same file: isa/rv32f.c

This is the task where you cannot lean on the host. Every instruction here has a rule that C either gets wrong or leaves undefined, and the reason is always the same: NaN, or the two zeros. Write the rules out before writing the code.

### Comparisons: one is quiet, two are signalling

All three write 0 or 1 into an x register, and all three return 0 when an operand is NaN. They differ only in whether that raises a flag:

```c
Instruction        NaN operand          Flag raised
feq.s              result 0             NV only for a signalling NaN (a quiet compare)
flt.s              result 0             NV for any NaN
fle.s              result 0             NV for any NaN
/* feq.s: quiet */
if (is_snan_bits(A) || is_snan_bits(B)) cpu_raise_fflags(c, FFLAG_NV);
int r = !is_nan_bits(A) && !is_nan_bits(B) && f_of(A) == f_of(B);

/* flt.s / fle.s: signalling */
if (is_nan_bits(A) || is_nan_bits(B)) {
    cpu_raise_fflags(c, FFLAG_NV);
    cpu_write_reg(c, i->rd, 0);
    return;
}
```

That is the pattern to internalise: test the bits for NaN first, then compare the floats. Note feq.s still gets -0 == +0 right for free, because the host's == does.

### Min and max: NaN is ignored, and the zeros are ordered

fmin.s/fmax.s are not "compare and select". The rules, in order:

1. A signalling NaN in either operand raises NV — whatever else happens.
2. Both NaN → the canonical NaN.
3. One NaN → return the other operand. A NaN never wins a min or a max.
4. Both zero → fmin.s(-0, +0) is -0 and fmax.s is +0. The host's < says they are equal, so this needs a bit trick: for two zeros, min is A | B and max is A & B — the sign bit does the work.
5. Otherwise compare as floats.

Rule 3 is the one that surprises people: fmin.s(qNaN, 1.0) is 1.0, with no flag at all.

### Conversions: saturate, never wrap

fcvt.w.s and fcvt.wu.s turn a float into an integer, and C's cast is undefined behaviour when the value is out of range or NaN. RISC-V defines it instead:

```text
Input                                         fcvt.w.s result            fcvt.wu.s result           Flag

NaN                                           0x7fffffff (max)           0xffffffff (max)           NV

too large                                     max                        max                        NV

too small / negative to unsigned              min / 0                    0                          NV

in range, fractional part                     rounded per rm             rounded per rm             NX
```

So: test for NaN, round in double (where every float is exact, so no flag is lost), range-check, then saturate. Never cast and hope.

The five static rounding modes must all work, and the test suite checks these ten values — derive them yourself before running it:

```text
           rne rtz rdn rup rmm
  2.5        2    2     2      3     3
 -2.5      -2    -2    -3    -2     -3
  3.5        4    3     3      4     4
```

2.5 and 3.5 under rne are the pair that proves ties go to even, not away from zero: 2 and 4. If your implementation gives 3 and 4, it is rounding half away from zero and only one of the two will be noticed.

The other direction, fcvt.s.w and fcvt.s.wu, is easier, run it through the host with the mode set, but it is not always exact: a 32-bit integer can need 31 significant bits and a float has 24, so large integers round and raise NX.

Checkpoint. ./rvsim tests/t2_conversions.s --batch must complete with cycles = 113, retired = 109, stalls = 0 and match --single-cycle. It exercises every rounding mode both statically and through fcsr.frm, so if one mode is wrong this is the test that says so.

## 8. Task 4 — Flags, fcsr, and Where It Lives in the Pipeline

The five flags are not decoration: they are how software finds out that a result it already holds is not the number it asked for. Each is raised by a specific situation, and cpu_raise_fflags ORs them into fcsr — sticky, never cleared by hardware.

```text
Flag    Raised when                                  Example

NV      the operation has no meaningful answer       0/0, inf - inf, sqrt(-1), any sNaN operand,
                                                     flt.s with a NaN, an out-of-range conversion

DZ      finite non-zero divided by zero              1.0 / 0.0 → +inf

OF      the rounded result is too large for the      FLT_MAX + FLT_MAX → +inf, with OF and NX
        format

UF      the result is subnormal and inexact          FLT_MIN * FLT_MIN → +0, with UF and NX

NX      the result had to be rounded                 almost every real calculation
```

Two pairings catch people out, and both are in the test suite:

- OF and UF always come with NX, because an overflowed or underflowed result is by definition not the exact answer.
- UF needs inexactness. min_subnormal * 1.0 is subnormal but exact, so it raises nothing at all. A naive "result is subnormal → UF" test fails this case.

And one that is purely a RISC-V rule rather than an IEEE one: fmadd.s(inf, 0, qNaN) raises NV. IEEE leaves the case to the implementation; RISC-V requires the flag, because inf * 0 is invalid regardless of what the addend is.

### Reading the flags back

The CSR instructions are given in isa/zicsr.c, and the assembler provides the pseudo-ops, so collecting flags per operation needs no new code:

```asm
     fsflags zero, zero             # clear all flags
     fdiv.s fs0, fs1, fs2           # the operation under test
     frflags s1                     # s1 = the flags it raised
```

frrm/fsrm do the same for the rounding mode, which is how t2_conversions.s tests the dyn mode: set frm with fsrm, then issue an instruction with no rounding-mode operand.

### Why fcsr needs no hazard logic

This is the part to be able to defend, because it looks like it should be a problem. fcsr is a register that nearly every FP instruction writes and the CSR instructions read — by the logic of Lab 3, that is a dependence, and it has no entry in src1/src2/src3.

It is safe for one structural reason: fcsr is read and written in EX, in program order, and an instruction in EX is never squashed afterwards. Branches resolve in EX too, so by the time an instruction reaches EX it is certain to retire. Instructions pass through EX one at a time in order, so each one sees every flag raised by every older instruction and none from any younger one. There is nothing to forward and nothing to stall for.

Contrast that with the f registers, which are written in WB and therefore need the whole forwarding apparatus. Same machine, two pieces of state, two different answers — and the difference is entirely about which stage does the writing.

### What to measure

Run every test program under both hazard units and record the table in Section 9. The point of the comparison is that FP code is unusually dependence-heavy — a Newton iteration or a Horner evaluation is a chain where every instruction consumes the previous result — so forwarding matters more here than anywhere else in the course:

```bash
./rvsim tests/t4_fp_program.s --batch                                  # forwarding: 159 cycles
./rvsim tests/t4_fp_program.s --batch --hazard stall                   # interlock: 261 cycles
```

That is a 39% reduction, on code you did not write and a pipeline you did not change.

## 9. Verification

### Step 1 — the pipeline must agree with the oracle, fcsr included

The single-cycle core runs your handlers too, so it is still the specification for values; what the pipeline adds is timing. Every FP program must produce identical x registers, identical f registers and an identical fcsr under both:

```bash
for f in tests/*.s; do
  ./rvsim $f --batch --single-cycle > ref.txt
  ./rvsim $f --batch               > got.txt
  diff <(grep -E '^(x|f)' ref.txt) <(grep -E '^(x|f)' got.txt) >/dev/null \

     && echo "$f MATCH" || echo "$f MISMATCH"
done
```

Or make test, which checks the goldens under both hazard units and cross-checks against RARS — 20 checks. Include the fcsr line in your own diffs: a flag raised at the wrong time is invisible in the register values.

### Step 2 — the numbers

```text
Program                    Instrs    forward cycles / stalls / forwards /     stall cycles / stalls / CPI
                                     CPI
t1_fp_arith.s              36        41 / 1 / 11 / 1.139                      56 / 16 / 1.556
t2_conversions.s           109       113 / 0 / 47 / 1.037                     167 / 54 / 1.532
t3_special_values.s        72        76 / 0 / 7 / 1.056                       85 / 9 / 1.181
t4_fp_program.s            133       159 / 14 / 61 / 1.195                    261 / 116 / 1.962

total                      350       389 / 15 / 126 / 1.111                   569 / 195 / 1.626
```

t4_fp_program.s also has 4 redirects (loop branches), so check the identity on it: 133 + 4 + 2×4 + 14 = 159. It prints 12000014141000 — a dot product of 120, Newton's method giving 1.4142135, and a Horner evaluation giving 1.0, each scaled by 1000 and converted to integers.

The 15 stalls under forwarding are all flw → FP-use: one in t1, fourteen in t4's dot-product loop. The 195 stalls under the interlock are what FP dependence chains cost a machine without forwarding.

### Step 3 — where the reference tool is wrong

make test cross-checks final register values against RARS 1.6, and RARS deviates from the RISC-V specification in four places. The golden files hold the spec-correct value and the affected registers are excluded from the RARS comparison. Do not "fix" your simulator to agree with RARS on these:

```text
Case                             RARS 1.6                           The spec (and this simulator)

arithmetic with a                returns the sNaN unchanged         canonical NaN 0x7fc00000, and NV
signalling-NaN operand           (0x7fa00000), raises nothing
fmadd.s(inf, 0, qNaN)            no NV                              NV required

fcvt.w.s of 2.5 with rne         3 (and -2.5 → -3), though 0.5      2 and -2 — ties to even
                                 →0

fcvt.wu.s of -2.5                raises NV and NX                   NV only — a clipped result sets
                                                                    invalid, not inexact
```

### What must match, and what may not

Every x register, every f register, fcsr, and every printed character must match the oracle. The cycle counts, stalls and forwards must match the table.

One defensible difference: if your fp_arith rounds through double where the reference rounds directly, you may see a different NX in a case where the double step is exact. If any flag differs from the table, say which operation produced it and why your reading of the spec supports it. A differing value is a bug.

## 10. Common Bugs

```text
Symptom                                            Likely cause

Assembler rejects fadd.s                           register_rv32f() not called in main.c, or the
                                                   mnemonic string is wrong — the assembler finds
                                                   instructions by the name you registered

An FP instruction decodes as illegal               funct3 registered as a value instead of -1 on an
                                                   instruction that carries a rounding mode there

Results right, but stalls and forwards are 0       the IF_*_FP flags are missing, so
                                                   control_from_instr() numbered the operands in
                                                   the x file and the hazard unit watched the wrong
                                                   registers

A fused multiply-add reads a stale third           IF_RS3_FP missing, so src3 is REG_NONE and the
operand                                            dependence is invisible

NaN results are 0xffc00000, not 0x7fc00000         the host's NaN was returned directly; canonicalise
                                                   every NaN result

A signalling NaN survives arithmetic unchanged no sNaN test — and note RARS agrees with the bug,
                                               so trust the golden file

fmin.s(qNaN, 1.0) returns NaN                      a NaN operand must be ignored when the other
                                                   operand is a number

fmin.s(-0, +0) returns +0                          comparing with <, which calls the zeros equal; two
                                                   zeros need the bit trick

feq.s raises NV on a quiet NaN                     feq.s is the quiet compare — only flt.s/fle.s
                                                   signal on any NaN

fcvt.w.s of 2.5 under rne gives 3                  rounding half away from zero; rne breaks ties to
                                                   even

Out-of-range conversion gives a wrapped            C's float-to-int cast is undefined out of range; test
value, or crashes                                  and saturate before casting

min_subnormal * 1.0 raises UF                      UF needs the result to be subnormal and inexact;
                                                   this one is exact

Symptom                                           Likely cause

OF or UF appears without NX                       they always come together; the given fflags_of
                                                  gets this right, so a hand-rolled flag path is the
                                                  suspect

Rounding mode has no effect                       fesetround never called, or called and not restored
                                                  — use the given fp_arith rather than the host's
                                                  default

rmm behaves exactly like rne                      the tie fix-up in d2f was bypassed. Test it with 1 +
                                                  2^-24

Mode 5, 6 or a bad frm produces a result          both must raise ILLEGAL_INSTRUCTION, and must
                                                  not write rd or any flag

Register dump shows the f registers as all zero   writing with cpu_write_reg instead of
                                                  cpu_write_freg

Strange optimised-only failures                   a pointer cast between uint32_t* and float*. Use
                                                  memcpy
```

## 11. Deliverables and Report

Submit:

1. isa/rv32f.c and your one-line change to main.c. Nothing else should have changed; say so explicitly if it has, and why.
2. The --batch output of all four test programs under --hazard forward, --hazard stall and --single-cycle, including the fcsr line.
3. A report of at most two pages answering the questions below.

### Report questions

1. Reproduce the table in Section 9 with your measured numbers. For any row that differs, state which design choice caused it and why it is defensible.
2. You added a second register file, a third source operand and a status register, and changed nothing in pipeline/. Explain the three design decisions from earlier labs that made that possible, and name the one thing that did have to be added to the pipeline in advance for fused multiply-adds to work.
3. fcsr is written by nearly every FP instruction and read by frflags, yet needs no forwarding and no stall. Explain why, in terms of which stage writes it, and contrast that with the f registers. What would have to change if fcsr were written in WB instead?
4. Derive the ten fcvt.w.s results for 2.5, -2.5 and 3.5 under the five rounding modes from the definitions of the modes, then show your measurements agree. Explain why 2.5 and 3.5 round differently under rne.
5. min_subnormal * 1.0 raises no flag, but FLT_MIN * FLT_MIN raises UF and NX. Both results are subnormal or smaller. Explain the difference precisely.

6. Explain why fneg.s of a signalling NaN keeps the payload and raises nothing, while fadd.s of the same value returns 0x7fc00000 and raises NV. What distinguishes the two instructions?
7. Pick one of the four RARS deviations in Section 9. Quote the sentence from the RISC-V F-extension specification that settles it, explain what RARS does instead, and say how you would decide which to trust if the test suite did not already tell you.
8. t4_fp_program.s runs in 159 cycles with forwarding and 261 with the interlock — a bigger relative gap than any integer program in the course. Explain what it is about floating-point code that produces that, using the structure of one of its three kernels as the example.

### Optional extension

The registry takes new instructions without any pipeline change — that is now demonstrated rather than asserted. Pick one and implement it: fmadd.s with a fifth operand would need a new format, so instead try fmv variants, a fclass-driven branch helper, or the double-precision fadd.d on a pair of registers. Say in advance which parts of the infrastructure you expect to have to touch, then report which ones you actually did.

## Appendix — The Special-Value Reference

This is the table t3_special_values.s checks, one case per line, each flag isolated. Work out the expected column before running it; every entry is a rule from Sections 3, 7 or 8.

```text
Operation                                 Result                   Flags
0.0 / 0.0                                 canonical NaN            NV
1.0 / 0.0                                 +inf                     DZ
FLT_MAX + FLT_MAX                         +inf                     OF, NX
FLT_MIN * FLT_MIN                         +0                       UF, NX
min_subnormal * 1.0                       min_subnormal            (none — exact)
inf - inf                                 canonical NaN            NV
qNaN + 1.0                                canonical NaN            (none — quiet)
sNaN + 1.0                                canonical NaN            NV
sqrt(-1.0)                                canonical NaN            NV
fmadd.s(inf, 0.0, qNaN)                   canonical NaN            NV (RISC-V rule)
fmin.s(qNaN, 1.0)                         1.0                      (none)
fmin.s(sNaN, 1.0)                         1.0                      NV
fmin.s(qNaN, qNaN)                        canonical NaN            (none)
fmin.s(-0.0, +0.0)                        -0.0                     (none)

Operation                                 Result                   Flags
fmax.s(-0.0, +0.0)                        +0.0                     (none)
feq.s(qNaN, qNaN)                         0                        (none — quiet compare)
flt.s(qNaN, 1.0)                          0                        NV
feq.s(sNaN, 1.0)                          0                        NV
fneg.s(sNaN)                              0xffa00000               (none — bit operation)
```

The last four lines are the ones worth staring at. Three instructions are handed the same kind of operand and behave three different ways: feq.s stays quiet unless the NaN is signalling, flt.s complains about any NaN, and fneg.s does not look at the value at all. None of that falls out of the host's floating-point unit — all three are rules you implement.

### Useful bit patterns

```text
+0.0               0x00000000          -0.0                0x80000000
1.0                0x3f800000          -1.0                0xbf800000
2.0                0x40000000          0.5                 0x3f000000
+inf               0x7f800000          -inf                0xff800000
canonical NaN      0x7fc00000          a signalling NaN 0x7fa00000
FLT_MAX            0x7f7fffff          FLT_MIN (normal) 0x00800000
min subnormal      0x00000001          1 + 2^-24 tie       (see t2_conversions.s)
```

The assembler's .float directive writes these for you — .float 1.5, -2.25, 1e10 — but .word 0x7fa00000 is how you get a signalling NaN into memory, since no decimal literal denotes one.
