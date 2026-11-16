# W10 implementation: RV32F and its missing starter infrastructure

W10 is implemented. This version adds the infrastructure absent from the uploaded
starter, rather than assuming the sheet's prepared core and pipeline already
exist. It includes all 26 requested RV32F instructions, the six CSR instruction
forms for fflags/frm/fcsr, and the eight RV32M instructions that the W10 sheet
assumes are given. Original W6–W9 timing and tests remain supported.

## Run

From riscv_sim_base:

```sh
make
make test
./rvsim tests/given/lab5_fp/f1_broad.s --single-cycle
./rvsim tests/given/lab5_fp/f1_broad.s --batch
./rvsim tests/given/lab5_fp/f3_kernel.s --hazard stall
./rvsim tests/given/lab5_fp/f3_kernel.s --predictor 2bit --hazard forward
```

The assembler records FP/CSR use from actual emitted instruction metadata.
Programs containing FP/CSR instructions default to the W10 reference pipeline:
forwarding plus the 2bit predictor. This applies to --batch and --hazard modes.
An explicit --single-cycle, --pipeline, --pipeline-forward or --pipeline-basic
keeps that chosen engine. Integer-only default behavior is unchanged. An explicit
--predictor still works as in W9. No input filename matching is used.

## What changed

- core/cpu_state.*: raw-bit fregs[32], writable f0, fcsr, accessors, FP ABI names
  and unified register reads/writes. FP dumps are shown only after FP/CSR activity,
  preserving the integer programs' output format.
- core/isa_registry.*: parallel operand metadata and fixed-rs2 matching, preserving
  existing six-field InstructionHandler initializers. FMT_R2, FMT_R4 and FMT_CSR
  augment the original formats. Generic-handler execution is metadata-selected.
- core/decoder.c: rs3/CSR fields, fixed-rs2 and R4 precision matching. Unsupported
  rs2 selections and non-single R4 precision encodings decode as illegal.
- assembler/assembler.*: FP register/ABI parsing, register-file validation, .float,
  optional rne/rtz/rdn/rup/rmm/dyn operands, FMA operands, CSR names/immediates and
  FP/CSR pseudo-ops. Also removes a pre-existing undefined negative left shift
  in li splitting, exposed by the new RV32M INT_MIN regression.
- isa/fp_helpers.*: rounding resolution, bit classification, numerical adapter.
- isa/rv32f.*: 26 FP handlers and registration metadata.
- isa/zicsr.*: csrrw/csrrs/csrrc/csrrwi/csrrsi/csrrci, limited to CSR 1/2/3.
- isa/rv32m.*: mul/mulh/mulhsu/mulhu/div/divu/rem/remu, including zero divisors and
  signed division overflow. Registered handlers also execute in the pipeline.
- pipeline/control.*, hazard.*, pipeline_state.h, pipeline.c: unified sources and
  destinations, a third captured/forwarded operand, generic handler execution,
  FP writeback selection, and precise fcsr staging.
- ui/cli_display.c and main.c: display, registration and default W10 selection.
- vendor/softfloat/: pinned binary32 subset with license and source provenance.
- tests/check_fp.c, tests/run_fp_tests.py, tests/fp_regression/: numerical,
  architecture, encoding, CSR, conversion and precise-exception checks.

FP pseudo-ops are fmv.s, fneg.s, fabs.s, frflags/frrm/frcsr,
fsflags/fsrm/fscsr and fsflagsi/fsrmi. Read forms take rd; write forms accept
source alone (discarding old value) or rd,source. Real CSR instructions take
rd,CSR,source. CSR immediate sources are constrained to 0..31. .float accepts
comma-separated binary32 decimal literals; .word specifies exact special bits.

## Numerical correctness

Instead of reconstructing a host-fenv helper with double-rounding and RMM corner
cases, the numerical adapter uses vendored Berkeley SoftFloat binary32 operations
with its RISCV specialization. Source is pinned at
07ebd29db0cfb55723a79fbbffe99103ef38e305; upstream notices and LICENSE.txt are
included. The subset is compiled locally; no download or installed FP package is
needed to build. Default compiler is GCC; C11 thread-local state is used (MSVC
uses its thread-storage spelling). The current platform configuration targets the
little-endian hosts used by this project; native MSVC execution remains unverified.

Rounding and flags are calculated using integer software arithmetic, not host
float approximations. This gives RNE/RTZ/RDN/RUP/RMM, single-round FMA, canonical
NaN results, signalling-NaN invalid flags, integer conversion saturation and
underflow detection after rounding. Each operation saves/restores backend TLS
rounding/flags/tininess state, so simulator instances do not inherit another
operation's numerical environment. .float parsing uses the host's binary32
literal conversion; arbitrary arithmetic does not.

Bit instructions preserve NaN payloads and do not raise numerical flags.
fmin/fmax ignore one NaN, canonicalize two NaNs, raise NV for signalling NaNs,
and explicitly order -0/+0. feq is quiet except for signalling NaNs; flt/fle
signal on any NaN. Float-to-integer conversion raises NV alone on clipping.
Arithmetic resolves rounding first: reserved static modes and dynamic frm>4
raise illegal instruction before destination or flag changes.

## Hazards and precise fcsr

Integer x1–x31 use dependency IDs 1–31; f0–f31 use 32–63. ID 0 means integer zero
or no dependency, so f0 remains an ordinary producer. Third-source metadata
makes FMA addends visible to load-use detection and both bypass paths. flw/fsw
have integer address bases with FP destination/store data. EX/MEM load addresses
remain excluded from data forwarding.

FP/CSR handlers run on a scratch CPUState populated with captured or forwarded
operands. Their destination result and next fcsr value travel through latches.
**Architectural fcsr commits at WB**, after all older instructions. EX obtains
its effective fcsr from the newest older staged EX/MEM or MEM/WB update, then
from committed state. This adds no stalls or register-forward counts, while
preserving dynamic-rounding and back-to-back CSR semantics.

This deliberately strengthens the sheet's suggested direct EX fcsr mutation:
a younger FP operation can compute behind an older fault or exit, but its staged
flags are discarded when the older terminal instruction reaches WB. The behavior
is tested against complete oracle state. Visible fcsr cannot be corrupted by a
squashed instruction or a younger instruction behind a precise exception.

## Measured supplied sample results

| Sample | Prints | Oracle cycles | Forward cycles / stalls / forwards | Stall cycles / stalls | Final fcsr |
|---|---|---:|---|---|---|
| f1_broad | 11 | 42 | 46 / 0 / 10 | 61 / 15 | 0x01 (NX) |
| f2_special | 12 | 66 | 71 / 1 / 12 | 90 / 20 | 0x11 (NV, NX) |
| f3_kernel | 1432536311 | 197 | 222 / 9 / 143 | 449 / 236 | 0x01 (NX) |

All eight supplied W10 golden runs match integer and FP register bits, fcsr,
program output, status, cycles and supplied counters. f2's stalling row has no
supplied golden; it is independently compared with the oracle. f3 executes
12 conditionals, with 6 misses and no indirect jumps under 2bit. Its identity is
197 + 4 + 9 + 2*6 = 222. Forwarding saves 50.6% of its stalling-mode cycles.
Accuracy for a run with zero conditional branches is reported as 100.0 by
convention, matching the W10 golden files.

## Verification

make test runs the original 747 W6–W9 Python checks, 9025 W10 checks, C predictor
assertions and directed FP/RV32M assertions. The W10 checks include 8875 independent
exact-rational vectors covering add/sub/mul/div/FMA, square root and both signed
and unsigned conversion directions under all five modes. The rational reference
uses Python integers/Fraction, not SoftFloat or host floating arithmetic. Square
roots are checked by comparing exact squared midpoints of adjacent floats.

Fourteen FP regression assembly programs check all CSR forms, dynamic rounding,
FMA third-operand forwarding, writable f0 and register-file separation, reserved
static/dynamic modes, unsupported CSRs, integer M edge behavior and precise
fault/exit/wrong-path flags. The C architectural checker compares all x/f bits,
fcsr and every RAM byte under twelve safe pipeline configurations. Assembler
negative tests reject wrong register files, malformed operand counts, invalid
rounding names and out-of-range CSR immediates.

These tests and supplied goldens are substantial evidence, not a proof of all
possible encodings or programs. RARS was not run locally; documented RARS NaN,
rounding and clipped-conversion deviations are not used as correctness targets.
Full privileged CSRs, FP traps, double precision, a RAS, caches and multi-cycle
floating functional units remain outside this educational model.

The final suite also passes an optimized -O1 -Werror build with AddressSanitizer
and UndefinedBehaviorSanitizer, including all fourteen FP/M regression programs.
