# W6–W8 implementation

The original single-cycle executor remains the architectural reference. No files
in core/, isa/, or ui/ were changed. W9/W10 are outside this implementation.

## Running

```sh
make
make test
./rvsim tests/sample1_sum_loop.s --batch
./rvsim tests/sample1_sum_loop.s --pipeline
./rvsim tests/sample1_sum_loop.s --pipeline-forward
./rvsim tests/micro.s --pipeline-forward --trace
./rvsim tests/given/lab3_hazards/h1_broad.s --hazard forward
```

Requires C11, Make and Python 3 (tests). Without flags the original interactive
debugger remains available. --batch / --single-cycle select the original batch
engine. Pipeline options imply batch; pipeline interaction is not implemented.
--trace writes clock traces to stderr; --max-cycles bounds clocks; --mem-size
retains its original meaning. Final pipeline PC is the fetch pointer, and is
excluded from architectural comparisons.

## Timing models

The W7/W8 sheets and the newly supplied tests describe different starter designs.
They are supported as explicitly selectable modes; fixtures are unchanged.

| Option | Timing | Hazard policy | Redirect cost |
|---|---|---|---|
| --pipeline | sheet | conservative three-latch stall | 1 |
| --pipeline-forward | sheet | forward + load-use stall | 1 |
| --pipeline-basic / --hazard none | reference | none (unsafe teaching mode) | 2 |
| --hazard stall | reference | two-latch stall, WB-to-ID read | 2 |
| --hazard forward | reference | forward + load-use stall | 2 |

Sheet mode fetches the redirected target in EX's redirect clock, killing ID.
Reference mode discards both younger slots and fetches the target next clock.
flushes counts redirect events; the reference goldens call this redirects and
report squashed = 2 * redirects. There is no branch prediction.

WB precedes ID reads. Sheet stalling deliberately still checks MEM/WB to preserve
the sheet's conservative policy; reference stalling only checks ID/EX and EX/MEM.
Sheet ecall reads committed a7/a0 in WB. Reference ecall treats a7/a0 as normal
latched/forwarded operands and carries them to WB. This reproduces the supplied
stall/forward statistics and the intentional wrong-answer none goldens.

## Code

- pipeline/control.h/.c: format-derived control signals and source-use metadata.
- pipeline/pipeline_state.h: four latches, valid bits, deferred faults, counters.
- pipeline/pipeline.c: snapshot/commit clock, five stages, ALU, sized signed and
  unsigned loads, stores, branches, jumps, WB syscalls, precise faults, trace.
- pipeline/hazard.h/.c: shared producer predicate, selectable stalling,
  EX/MEM-before-MEM/WB operand forwarding, one-cycle load-use detection.

Each stage reads old latches and writes next latches. A stall freezes fetch and
IF/ID and injects an EX bubble. Redirects beat younger hazards and faults.
Forwarded values feed ALU, store data, branch comparisons and jalr targets.
Load addresses are never forwarded as data. Invalid latches, x0 writes and unused
sources do not create hazards. Forwards count operands, not instructions.

Faults only become visible at WB. Terminal WB processing precedes younger stores,
so wrong-path faults, speculative fetch past exit, and younger side effects cannot
corrupt the architectural result.

main.c wires modes. Makefile adds sources, header dependencies and regressions.
The only assembler change adds la as the existing two-instruction absolute li
expansion in both passes, needed by the supplied tests. Existing li semantics and
instruction sizes remain unchanged. This is an explicit departure from the
sheet's assumed starter, which already had la.

## Measured results

Sheet mode:

| Program | Retired | Stall cycles / stalls / flushes | Forward cycles / stalls / flushes | Forward EX/MEM / MEM/WB |
|---|---:|---|---|---|
| sample1_sum_loop | 53 | 108 / 40 / 11 | 68 / 0 / 11 | 13 / 11 |
| sample2_mem_and_branches | 13 | 26 / 8 / 1 | 18 / 0 / 1 | 2 / 2 |
| micro | 5 | 15 / 6 / 0 | 9 / 0 / 0 | 2 / 1 |

CPI = cycles/retired. Forwarding reduces cycles by 37%, 31%, 40% respectively.
The single-cycle model counts one instruction per clock; these are cycle-count
comparisons, not elapsed-time speedups with modeled physical clock periods.

Reference no-hazard pipeline:

| Program | Retired | Cycles | Redirects |
|---|---:|---:|---:|
| p1_broad | 91 | 113 | 9 |
| p2_edge | 43 | 57 | 5 |
| p3_kernel | 658 | 762 | 50 |

Reference hazard modes:

| Program | Retired | Stall cycles / stalls | Forward cycles / stalls / forwards |
|---|---:|---|---|
| h1_broad | 43 | 84 / 33 | 52 / 1 / 21 |
| h2_edge | 30 | 53 / 17 | 40 / 4 / 11 |
| h3_kernel | 321 | 749 / 324 | 461 / 36 / 191 |

All supplied W7/W8 golden register values, program output, status, cycles, CPI,
and supplied counters match, including deliberate wrong results/illegal halts in
none mode. None mode is unsuitable for general execution.

RAW dependencies are the data hazards here: in-order reads and writes prevent
WAR/WAW. The conservative distance-1/2/3 costs are 3/2/1 stalls. With forwarding,
ALU results need no stalls, while immediate load consumers need one because data
arrives at the end of MEM after EX needed it. Store-data load-use also costs one;
a dedicated MEM-to-MEM bypass, not implemented in W8, could remove that case.
EX/MEM priority ensures the youngest producer wins when writes target the same
register. Branch comparisons and jalr need the same bypassing as arithmetic.

## Accounting edge case

For sheet examples: cycles = retired + stalls + flushes + 4, exactly.
Reference timing uses 2 * flushes instead.

The supplied s3_control has another function immediately after exit. In stalling
modes a younger ID stall overlaps exit's final drain without delaying exit.
The physical stall is counted under the sheet's 'any cycle ID held' definition.
discarded_stalls records stalls belonging to younger unretired instructions on
termination. Subtract this counter (1 for that fixture) from the accounting
identity. It is computed from latch metadata, with no fixture-specific adjustment.
Arbitrary younger redirects can also overlap termination; the simple identity
assumes there are no such speculative terminal events.

## Verification

make test passes 444 checks, covering the two base examples, micro, and nine
supplied W6–W8 programs across all safe modes; all 18 supplied W7/W8 golden runs;
sheet counters; traces; and cycle limits. A C differential harness compares all
32 registers and every memory byte, including tests of precise illegal/fetch/load
faults, exit suppression of younger stores, redirect priority and loads into x0.

The suite also passes at -O1 with -Werror, AddressSanitizer and
UndefinedBehaviorSanitizer. The normal -Wall -Wextra build is warning-free.
RARS was not installed or run; supplied goldens supply independent expectations.

The exact sample3_hazards and sample4_data_hazards sources referenced in the sheets
are absent from both uploads. They were not fabricated, and their exact timing
rows are not claimed as verified. Supplied p*/h* programs cover the intended
features. W9/W10 fixtures remain in the extracted archive, outside this scope.
