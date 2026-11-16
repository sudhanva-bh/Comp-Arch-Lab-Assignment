# RV32I Architecture Labs: Weeks 6–10

A modular RV32I processor simulator written in C11. The project progresses from
single-cycle execution to a synchronous five-stage pipeline, data forwarding,
and branch prediction. Labs 1–5 are implemented, including RV32F, its CSR support and the
RV32M starter extension. Source code lives in [`riscv_sim_base/`](riscv_sim_base/).

The pipeline is cycle-accurate for its documented educational timing models.
It does not simulate gates, clock frequency, caches, or a complete privileged
RISC-V platform. The supported base instructions are those listed below; Only fflags/frm/fcsr CSRs are implemented; other privileged CSR behavior is outside scope.

## Choose a document

| Document | Purpose |
|---|---|
| [tutorial.md](tutorial.md) | Build, run individual sample tests, run the full suite, compare results and debug traces |
| [COMPREHENSIVE_LAB_GUIDE.md](COMPREHENSIVE_LAB_GUIDE.md) | Implementation details, latch timing, hazards, prediction, and the implemented FP architecture |
| [LAB_SHEETS_EXPLAINED.md](LAB_SHEETS_EXPLAINED.md) | Start from zero: processor concepts and what each week's lab asks you to build |
| [W6–W8 implementation record](riscv_sim_base/W6_W8_IMPLEMENTATION.md) | Earlier measured results and implementation decisions |
| [W10 implementation record](riscv_sim_base/W10_IMPLEMENTATION.md) | Recreated starter, numerical backend, precise flags, measured results and verification |
| [W9 implementation record](riscv_sim_base/W9_IMPLEMENTATION.md) | Predictor design and measured W9 results |

## Implemented progression

| Lab | Week | Result |
|---|---|---|
| 1 | W6 | 32 integer registers, x0 hardwired to zero, default 1 MiB byte-addressable RAM, two-pass assembler, registry dispatch, single-cycle oracle |
| 2 | W7 | IF/ID/EX/MEM/WB pipeline, four valid-bit latches, ID controls, EX redirects, WB-visible faults |
| 3 | W8 | Selectable stalling/forwarding, operand bypassing, one-cycle load-use interlock, counters |
| 4 | W9 | Four direction predictors, IF direct-jump handling, EX prediction checking, accuracy counters |
| 5 | W10 | FP registers, fcsr, typed operand metadata, exact rounding/flags, CSR access, third-source forwarding and 26 RV32F handlers |

All supported terminating examples use `ecall`: a7=1 prints signed a0; a7=10 exits.
The single-cycle model counts one clock per executed instruction, including exit,
so normal successful runs have $\mathrm{CPI}=1$. This is a functional reference,
not evidence of equal hardware clock periods across engines.

## Build and run

### Linux, WSL, or another GCC/Make environment

From this repository root:

```bash
cd riscv_sim_base
make
make test
./rvsim tests/sample1_sum_loop.s --pipeline-forward
./rvsim tests/given/lab4_branch/b1_patterns.s --predictor 2bit
```

Requires GCC (or a compatible C11 compiler), Make, and Python 3 for tests.
`make test` builds both C verifiers before starting the Python harness. Running
`python3 tests/run_tests.py` directly requires those binaries to exist first.

### Native Windows: MSVC

Use **Developer PowerShell for Visual Studio 2022** with the C++ build tools
installed. Run from the repository root:

```powershell
Set-Location riscv_sim_base
$sources = Get-ChildItem core,isa,assembler,ui,pipeline,vendor/softfloat/source,vendor/softfloat/RISCV -Filter *.c |
    ForEach-Object { $_.FullName }
cl /nologo /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS /Icore /Iisa /Iassembler /Iui /Ipipeline /Ivendor/softfloat/include /Ivendor/softfloat /Ivendor/softfloat/RISCV /DSOFTFLOAT_FAST_INT64 /DINLINE_LEVEL=0 $sources main.c /Fe:rvsim.exe
.\rvsim.exe tests\sample1_sum_loop.s --pipeline-forward
```

MSVC commands are provided for the C11 source layout; they have not been executed
on Windows in this environment. The bundled Makefile uses GCC options and is not
an NMake makefile. For the exact tested build/test workflow on Windows, use WSL
and the Linux commands above. Native verifier build commands are in the tutorial.

## CLI reference

```text
rvsim <program.s> [options]
```

| Option | Behavior |
|---|---|
| No engine option | Integer-only: original debugger; FP/CSR program: W10 pipeline |
| `--batch` | Integer-only: original batch engine; FP/CSR program: reference forward+2bit unless explicitly overridden |
| `--single-cycle` | Explicit original batch engine |
| `--pipeline` | W7/W8 sheet timing, conservative three-latch stalling |
| `--pipeline-forward` | Sheet timing, forwarding and load-use detection |
| `--pipeline-basic` | Reference timing, no hazards; intentionally unsafe teaching mode |
| `--hazard stall` | Reference timing, two-latch interlock with same-cycle WB-to-ID reads |
| `--hazard forward` | Reference timing, forwarding and load-use detection |
| `--hazard none` | Reference timing, no hazards; reproduces intentionally wrong goldens |
| `--predictor NAME` | W9: `not-taken`, `btfn`, `1bit`, `2bit`; implies reference timing and forwarding unless `--hazard` is supplied |
| `--trace` | Pipeline clock trace to stderr; has no effect in the original engine |
| `--max-cycles N` | Batch clock limit; default 1,000,000 |
| `--mem-size N` | Simulated RAM bytes; default 1,048,576 |

Pipeline selectors imply batch execution. `--predictor` accepts `--hazard` before
or after it, but rejects combination with an explicit legacy engine selector
(`--single-cycle`, `--pipeline`, `--pipeline-forward`, `--pipeline-basic`). There
is no implicit W9 mode: select `--predictor 2bit` explicitly. Avoid mixing other
engine selectors; most legacy selections are processed in argument order.
Pipeline stepping is not provided by the interactive debugger. FP/CSR use is
detected by assembler instruction metadata, not filenames; `--single-cycle`
explicitly selects the oracle even for FP programs.

## Source tree

Generated binaries (`rvsim`, `pipeline_checks`, `predictor_checks`, `fp_checks`) and `.o`/`.d`
files are omitted here. `*.h`/`*.c` denotes the actual paired header/source files.

```text
riscv_sim_base/
├── main.c                       CLI, ISA setup, assembly and engine selection
├── Makefile                     GCC build, header dependencies and test orchestration
├── README.md                    Entry link back to the current root documentation
├── W6_W8_IMPLEMENTATION.md       Historical W6–W8 implementation/measurement record
├── W9_IMPLEMENTATION.md          W9 design, measurements and verification record
├── W10_IMPLEMENTATION.md         FP infrastructure, numerical backend and measurements
├── core/
│   ├── cpu_state.h / .c         Integer state, ABI names, x0 accessors, status
│   ├── memory.h / .c            Bounds-checked little-endian byte/half/word RAM
│   ├── isa_registry.h / .c      Instruction formats, handler registry and lookup
│   ├── decoder.h / .c           Decode fields and reconstruct signed immediates
│   └── executor.h / .c          Original single-cycle reference executor
├── isa/
│   ├── rv32i_base.h / .c        Base instruction handlers and registration
│   ├── rv32m.h / .c             Eight integer multiply/divide handlers
│   ├── rv32f.h / .c             26 floating-point handlers
│   ├── fp_helpers.h / .c        Exact numerical adapter and rounding validation
│   └── zicsr.h / .c             Six CSR forms for fflags/frm/fcsr
├── assembler/
│   └── assembler.h / .c         Two passes, labels, encoding, directives and pseudo-ops
├── ui/
│   └── cli_display.h / .c       State display and original interactive debugger
├── pipeline/
│   ├── control.h / .c           Format-derived control and source-use signals
│   ├── pipeline_state.h         IFID, IDEX, EXMEM, MEMWB and Pipeline structures
│   ├── pipeline.c               Five-stage clock, redirects, faults, trace and summary
│   ├── hazard.h / .c            RAW interlock and operand forwarding
│   ├── branch_predictor.h / .c  Predictor interface, table index and name lookup
│   ├── bp_static.c              Always-not-taken and BTFN implementations
│   └── bp_dynamic.c             One-bit and two-bit implementations
├── tests/
│   ├── sample1_sum_loop.s       Original sum example: prints 55
│   ├── sample2_mem_and_branches.s
│   ├── micro.s                  Five-instruction dependency trace
│   ├── run_tests.py             747 Python checks, goldens, CLI and timing checks
│   ├── check_state.c            Full-memory/register oracle comparison verifier
│   ├── check_predictor.c        State transitions, reset, aliasing and isolation assertions
│   ├── check_fp.c               Directed FP/M checks and numerical probe
│   ├── run_fp_tests.py          9025 W10 checks, including 8875 independent rational vectors
│   ├── fp_regression/           Fourteen FP/CSR/M precise-state programs
│   ├── regression/              Eight focused assembly corner cases
│   └── given/
│       ├── Cycles references.md Supplied cross-lab reference figures
│       ├── lab1_skeleton/       s1–s3 baseline samples
│       ├── lab2_pipeline/       p1–p3 plus expected/ goldens
│       ├── lab3_hazards/        h1–h3 plus expected/ goldens
│       ├── lab4_branch/         b1–b3 plus expected/ goldens
│       └── lab5_fp/             f1–f3 plus expected/; executable W10 samples
├── vendor/softfloat/             Pinned binary32 numerical backend, sources and license
└── results/
    ├── verification.txt         Recorded standard/sanitizer verification
    └── w9/                      Captured single-cycle and predictor/hazard runs
```

The eight regressions are `exit_store`, `load_zero`, `precise_fetch`,
`precise_illegal`, `precise_memory`, `predicted_loop_exit`, `redirect_priority`,
and `wrong_path_branch` (all `.s`). All 15 supplied samples are present and executable.

## Measured performance

### W7/W8 sheet timing

| Program | Single-cycle | Stalling cycles / CPI | Forwarding cycles / CPI | Cycle reduction from stalling |
|---|---:|---|---|---:|
| sample1_sum_loop | 53 | 108 / 2.038 | 68 / 1.283 | 37.0% |
| sample2_mem_and_branches | 13 | 26 / 2.000 | 18 / 1.385 | 30.8% |
| micro | 5 | 15 / 3.000 | 9 / 1.800 | 40.0% |

### Supplied W8 reference timing

| Program | Single-cycle | Stalling | Forwarding | Forward load-use stalls |
|---|---:|---:|---:|---:|
| h1_broad | 43 | 84 | 52 | 1 |
| h2_edge | 30 | 53 | 40 | 4 |
| h3_kernel | 321 | 749 | 461 | 36 |

### Supplied W9 forwarding cycles

| Program | Single-cycle | W8 reference stalling, no prediction | W8 reference forwarding, no prediction | not-taken | btfn | 1bit | 2bit |
|---|---:|---:|---:|---:|---:|---:|---:|
| b1_patterns | 1330 | 2528 | 1864 | 1856 | 1432 | 1506 | 1476 |
| b2_adversarial | 4739 | 13461 | 5367 | 5287 | 5007 | 5033 | 4985 |
| b3_applied | 3195 | 6268 | 4658 | 3986 | 3986 | 3970 | 3982 |

Even W9 `not-taken` improves on W8 forwarding: IF resolves direct `jal`, removing
two redirect cycles per direct jump. Speedups here describe modeled cycles,
not measured wall-clock execution time or hardware clock frequency.

## Verification and scope

`make test` passes **747 W6–W9 checks plus 9025 W10 checks**, and C predictor
and directed FP/RV32M assertions. W10 includes 8875 independent exact-rational
rounding vectors under all five modes, all eight supplied FP golden runs, and
complete x/f/fcsr/RAM comparisons across twelve safe configurations.

The suite passes `-O1 -Werror` with AddressSanitizer and UndefinedBehaviorSanitizer.
Tested safe-mode state matches the oracle; this is evidence for the tested cases,
not a formal proof over every possible program. Native MSVC and RARS were not
run here. Unsafe `none` mode intentionally permits dependency errors.

### W10 measured results

| Program | Oracle | Forward cycles / stalls / forwards | Stall cycles / stalls | Prints |
|---|---:|---|---|---|
| f1_broad | 42 | 46 / 0 / 10 | 61 / 15 | 11 |
| f2_special | 66 | 71 / 1 / 12 | 90 / 20 | 12 |
| f3_kernel | 197 | 222 / 9 / 143 | 449 / 236 | 1432536311 |

The numerical backend is a vendored Berkeley SoftFloat binary32 subset with
RISC-V specialization and retained license/provenance. No network or separately
installed numerical package is needed to build. FP/CSR effects compute in EX,
forward internally, and commit fcsr only at WB for precise exceptions. See
[W10_IMPLEMENTATION.md](riscv_sim_base/W10_IMPLEMENTATION.md).

## Supported assembly and next step

The base subset includes integer arithmetic/logic and shifts; `lui`, `auipc`;
`lb/lh/lw/lbu/lhu`; `sb/sh/sw`; six conditional branches; `jal/jalr`; and `ecall`.
Pseudo-ops: `nop`, `ret`, `j`, `mv`, `li`, `la`. Both `li` and `la` always expand
to two instructions. Directives: `.text`, `.data`, `.globl`, `.word`, `.half`,
`.byte`, `.align`. Sections share one flat address space; `.align n` aligns to
an n-byte boundary, not a power-of-two exponent.

RV32M adds mul/mulh/mulhsu/mulhu/div/divu/rem/remu. RV32F adds the 26 instructions
listed in the comprehensive guide, with FP register/ABI syntax, `.float`, static
or dynamic rounding, sign/move pseudo-ops, and fflags/frm/fcsr access. f0 is
writable; x0 remains zero. Other CSRs and double precision are outside scope.
