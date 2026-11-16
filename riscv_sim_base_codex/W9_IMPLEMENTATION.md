# W9: branch prediction

W9 is implemented on top of the W8 project. All previous execution modes remain
unchanged. W10 floating-point support is not part of this project version.

## Build and use

Requires a C11 compiler, Make, and Python 3 to run tests.

```sh
make
make test
./rvsim tests/given/lab4_branch/b1_patterns.s --batch --predictor not-taken
./rvsim tests/given/lab4_branch/b1_patterns.s --batch --predictor btfn
./rvsim tests/given/lab4_branch/b1_patterns.s --batch --predictor 1bit
./rvsim tests/given/lab4_branch/b1_patterns.s --batch --predictor 2bit
./rvsim tests/given/lab4_branch/b3_applied.s --predictor 2bit --hazard stall
./rvsim tests/given/lab4_branch/b1_patterns.s --predictor 2bit --trace
```

Selecting --predictor enables W9 with reference timing and forwarding by default.
--hazard stall/forward may be placed before or after --predictor. An explicitly
selected legacy engine (--single-cycle, --pipeline, --pipeline-forward or
--pipeline-basic) cannot be combined with --predictor; incompatible options are
rejected. For backward compatibility, --batch alone still runs the original
single-cycle engine: select --predictor 2bit explicitly for the lab's default W9
configuration. --max-cycles, --mem-size and --trace remain supported.

## Files and behavior

- pipeline/branch_predictor.h: the handout's BranchPredictor interface, 64-byte
  BPState table and index function. predict reads state; update is the only
  learning operation. There is no global mutable predictor state.
- pipeline/branch_predictor.c: lookup for all four predictor names.
- pipeline/bp_static.c: always-not-taken and backward-taken/forward-not-taken.
- pipeline/bp_dynamic.c: one-bit last-outcome and two-bit saturating counters.
- pipeline/pipeline_state.h and pipeline/pipeline.c: state owned by each pipeline,
  predicted-next-PC latches, fetch prediction, EX resolution, updates and counters.
- main.c and Makefile: selection and build integration.

This extends the W8 starter because the upload did not contain the pre-wired W9
infrastructure assumed by the assignment. No core/, isa/, assembler/ or ui/
changes were needed for W9; the prior W8 la assembler alias is retained.

IF reuses the decoder's target-immediate extraction. Conditional branches consult
the selected direction predictor using only PC and target; direct jal targets are
selected immediately. Other instructions fall through. Predictions stay frozen
with IF/ID during a stall and travel into ID/EX. EX compares the actual next PC
against the stored guess and updates the predictor from the actual outcome.
Updates made in EX are visible to a fetch in the same clock, matching supplied
reference results. IF never reads register operands for prediction.

Correct predictions cost no control cycles. A misprediction squashes both younger
slots and redirects fetch on the following clock. A direct jal has no penalty;
jalr is deliberately not predicted and always redirects in EX at a two-cycle
cost, even if its actual target happens to equal fall-through. The link value is
still PC+4. Redirects beat younger stalls/faults. Wrong-path branches cannot train
or increment resolved counters, and wrong-path writes/stores cannot commit.

The one-bit table starts not taken. Two-bit counters start at 1 (weakly not taken),
predict taken at 2 or 3, and saturate at 0 and 3. Index is (pc >> 2) & 63; addresses
separated by 256 bytes alias with no tags. Reset and updates affect only the
pipeline's own table, so multiple pipeline instances can coexist.

## Measurements from supplied tests

All register values, printed characters, cycles, CPI and counters match the
provided golden files under forwarding. Single-cycle goldens match as well.

| Program | Retired | Branches | Predictor | Misses | Accuracy | Redirects | Cycles | CPI |
|---|---:|---:|---|---:|---:|---:|---:|---:|
| b1_patterns | 1330 | 328 | not-taken | 261 | 20.4% | 261 | 1856 | 1.395 |
| b1_patterns | 1330 | 328 | btfn | 49 | 85.1% | 49 | 1432 | 1.077 |
| b1_patterns | 1330 | 328 | 1bit | 86 | 73.8% | 86 | 1506 | 1.132 |
| b1_patterns | 1330 | 328 | 2bit | 71 | 78.4% | 71 | 1476 | 1.110 |
| b2_adversarial | 4739 | 364 | not-taken | 272 | 25.3% | 272 | 5287 | 1.116 |
| b2_adversarial | 4739 | 364 | btfn | 132 | 63.7% | 132 | 5007 | 1.057 |
| b2_adversarial | 4739 | 364 | 1bit | 145 | 60.2% | 145 | 5033 | 1.062 |
| b2_adversarial | 4739 | 364 | 2bit | 121 | 66.8% | 121 | 4985 | 1.052 |
| b3_applied | 3195 | 510 | not-taken | 145 | 71.6% | 336 | 3986 | 1.248 |
| b3_applied | 3195 | 510 | btfn | 145 | 71.6% | 336 | 3986 | 1.248 |
| b3_applied | 3195 | 510 | 1bit | 137 | 73.1% | 328 | 3970 | 1.243 |
| b3_applied | 3195 | 510 | 2bit | 143 | 72.0% | 334 | 3982 | 1.246 |

Every forwarding row obeys cycles = retired + 4 + stalls + 2*redirects and
redirects = mispredicts + jalr. b1/b2 have no stalls or jalr. b3 has 115 load-use
stalls and 191 jalr, accounting for an unavoidable 382 control cycles in this
model. The direction predictor can only reduce the remaining 2*mispredicts term.
A perfect direction oracle would have zero misses; the observed exit/warm-up
miss counts are limitations of these predictors, not a universal theoretical
lower bound for all prediction mechanisms.

BTFN wins the aggregate cycle count here (10425), slightly ahead of 2bit (10443).
For b1, loop direction makes the static rule especially useful; dynamic tables
also pay for training and interference. In b2, 2bit beats 1bit because hysteresis
reduces re-entry misses and resists interference. Alternation defeats simple
last-outcome prediction, while deliberate 256-byte/512-byte collisions make
branches train the same entry. b3 favors 1bit slightly, but all four retain the
same jalr and load-use costs. Dynamic prediction is workload-dependent.

With --hazard stall, all architectural results still agree with the oracle.
b3 has one younger ID stall overlapping terminal drain: its inherited
`discarded_stalls` counter is 1. As documented for W8, subtract this overlap when
applying the simple cycle identity to that stalling configuration. Raw stall
counts still count every ID-held cycle. See results/w9 for actual run outputs.

## Verification

make test runs 747 Python checks plus C predictor assertions. It includes all
prior W6–W8 checks, the 15 W9 supplied goldens, all four predictors with both safe
hazard policies, complete register/memory comparisons, wrong-path branch counts,
a predicted-taken loop exit, cycle bounds and option validation/order checks.
C assertions cover every two-bit transition and saturation endpoint, reset,
const prediction, aliasing, lookup and isolation between pipeline instances.

The four original t1/t2/t3/t4 W9 example sources are not in the uploads; their
exact handout rows are not claimed as measured. Supplied b1/b2/b3 tests cover
loop patterns, adversarial aliasing and realistic calls, loads and branches.
No RARS installation was used; the provided goldens are independent expected
results in addition to comparisons against the single-cycle implementation.

The complete suite also passes an optimized -O1 -Werror build with
AddressSanitizer and UndefinedBehaviorSanitizer enabled.
