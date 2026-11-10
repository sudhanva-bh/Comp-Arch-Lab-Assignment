<!-- Page 1 -->

Lab Sheet 4: Branch Prediction on the Pipelined RV32I Simulator

1. Objectives
Be clear about what you are building, because it is a smaller thing than it looks. Your pipeline already
handles branches correctly. It resolves them in EX, squashes the two younger instructions, and
refetches from the right address — that machinery is in pipeline.c from the previous labs and it is
given. What is missing is a guess: something that tells IF where to fetch next, three cycles before the
answer is known.

So your unit answers one question, once per fetched conditional branch, from the PC and the target
alone: Taken, or not taken?

And it answers a second one in EX, after the real outcome arrives: what should I remember for next
time? Everything else — redirecting, squashing, counting — the pipeline already does.

Concretely you will:

       1. Work against the given BranchPredictor interface: a predict consulted in IF, an update called
          in EX, and a small table of state owned by the pipeline.
       2. Write the two static predictors: always-not-taken, and backward-taken/forward-not-taken.
       3. Write the 1-bit dynamic predictor: remember each branch's last outcome.
       4. Write the 2-bit saturating counter, and explain the one case where it beats the 1-bit version and
          the one case where neither helps.
       5. Measure prediction accuracy and CPI for all four, and account for every cycle the pipeline loses
          to control.

2. What You Start From
You need your working pipeline and hazard unit from the previous labs. Check them before writing any
new code:
cd lab3
make
./rvsim tests/t1_counted_loop.s --batch --single-cycle                            # oracle: 37 cycles
./rvsim tests/t1_counted_loop.s --batch --predictor not-taken                     # 59 cycles, 9
mispredicts
The second line is where the previous lab left you: a loop whose branch is taken nine times out of ten,
guessed wrong every single time. Nine mispredicts at two cycles each is 18 of those 59 cycles thrown
away. By the end of this sheet the same program runs in 43.

What is given, and what you write
The predictor is a plug-in, exactly like the hazard unit. The interface, the lookup table and all the pipeline
wiring are given; you write two files.

File                                          Given or yours

---

<!-- Page 2 -->

File                                          Given or yours
pipeline/branch_predictor.h                   given: BPState, bp_index(), BranchPredictor
pipeline/branch_predictor.c                   given: the predictor table and bp_find()
pipeline/bp_static.c                          Task 1 — yours (not-taken, btfn)
pipeline/bp_dynamic.c                         Tasks 2 and 3 — yours (1bit, 2bit)
pipeline/pipeline.c                           given: pre-decodes in IF, calls predict, carries the guess
                                              down the pipe, calls update in EX, flushes on a mispredict,
                                              counts everything

The predictor is chosen at run time, and the hazard unit still is too:
./rvsim prog.s --batch --predictor not-taken
./rvsim prog.s --batch --predictor btfn
./rvsim prog.s --batch --predictor 1bit
./rvsim prog.s --batch --predictor 2bit        # the default
./rvsim prog.s --batch --predictor 2bit --hazard stall

Rules of engagement
       •   core/, isa/, assembler/ and ui/ stay untouched. Read them freely.
       •   Do not change branch_predictor.h, branch_predictor.c or pipeline.c. If you want to, you
           are solving the wrong problem: predict already receives everything a direction predictor can
           legitimately use.
       •   All four predictors stay selectable. The comparison between them is the lab.
       •   Your predictor sees the branch's PC and its target address — nothing else. No register values,
           no condition codes, no knowledge of whether the branch will actually be taken. If it could see
           that, it would not be a prediction.

3. Background: The Cost of a Wrong Guess
IF has to fetch something every cycle. For an ordinary instruction the next address is pc + 4; for a
conditional branch it is pc + 4 or the target, and which one is not known until EX computes the
condition — three cycles later. The pipeline cannot wait, so it guesses, fetches along the guessed path,
and finds out in EX whether it was right.
       IF ---- pre-decode the fetched word ------------------------+
               conditional branch? ask predict(pc, target)         |
               jal?                 target is in the immediate     | the guess
               anything else?       pc + 4                         | travels down
                                                                   | the pipe as
       ID                                                          | pred_next
                                                                   |
       EX ---- compute the real next PC                            |
               conditional branch? call update(pc, target, taken) <+
               real next PC != pred_next? squash IF and ID, refetch

---

<!-- Page 3 -->

The pre-decode in IF reads only the opcode and the immediate — a few gates in hardware. It stands in
for a branch target buffer that always hits, so this lab is about predicting the direction of a branch, never
its address.

Three outcomes, three prices
What IF did                              What EX found                 Cost

guessed the right path                   guess matches                 0 cycles — the pipeline never
                                                                       notices

guessed the wrong path                   guess differs                 2 cycles — the instructions in IF
                                                                       and ID are squashed and fetch
                                                                       restarts

jal: redirected in IF from the pre-      always matches                0 cycles
decoded target

jalr: not predicted at all, fetch        almost always differs         2 cycles, every time
continued at pc + 4

Two of these rows are worth dwelling on, because they are the difference between this lab and the last
one.

jal became free. In the previous lab every taken branch and every jump cost 2 cycles. Here IF pre-
decodes a jal and redirects itself immediately, with no guess and no risk — the target is right there in
the immediate field. On t4_calls.s, which executes 33 jals, that one change alone saves 66 cycles
before any prediction happens at all.

jalr is still never free. A ret is a jalr, and its target comes from a register that IF cannot read. There is
no return-address stack in this design, so every return costs 2 cycles whatever predictor you select. In
t4_calls.s that is 18 returns — 36 cycles, and it is why all four predictors score almost the same on
that program.

What redirects now counts
The cycle identity from the previous labs is unchanged:
\text{cycles} = \text{retired} + 4 + 2 \times \text{redirects} + \text{stalls}
but redirects has quietly changed meaning. It used to be every taken branch and jump. It is now every
mispredict, which is:
redirects = conditional-branch mispredicts + jalr count
Check that on the statistics block of any run. On t4_calls.s with 2bit: 2 mispredicts + 18 jalr = 20
redirects, and 33 jal contribute nothing. A predictor can only ever shrink the first term.

4. Worked Example: One Loop, Four Guesses
tests/t1_counted_loop.s is the whole lab in nine instructions:

---

<!-- Page 4 -->

.text
.globl main
main:
     addi t0, zero, 10            # 0x00 counter
     addi t1, zero, 0             # 0x04 sum
loop:
     add t1, t1, t0               # 0x08
     addi t0, t0, -1              # 0x0c
     bne t0, zero, loop           # 0x10 backward branch, target 0x08
     mv   a0, t1                  # 0x14
     ...
The bne executes ten times, and its outcome sequence is the only thing that matters:
T   T   T   T   T   T   T   T   T   N
Nine takens then one not-taken. Now count the misses each predictor makes, by hand, before running
anything:

Predictor           Reasoning                                                              Misses
not-taken           guesses N every time; wrong on all nine takens                         9
btfn                target 0x08 < pc 0x10, so backward → guesses T every time; wrong       1
                    only on the exit
1bit                table starts at not-taken, so the first execution misses; then it      2
                    predicts T correctly until the exit misses
2bit                counter starts weakly-not-taken, so the first execution misses; it     2
                    then saturates at taken and only the exit misses

Measured: 9, 1, 2, 2. Cycles: 59, 43, 45, 45, against 37 for the single-cycle oracle.

What a mispredict looks like
Run ./rvsim tests/t1_counted_loop.s --batch --trace --predictor 2bit and find cycles 7 and
8:
[cycle 7] IF 0x00000018 | ID 0x00000014 | EX 0x00000010 | MEM 0x0000000c | WB
0x00000008
[cycle 8] IF 0x00000008 | ID ---------- | EX ---------- | MEM 0x00000010 | WB
0x0000000c
In cycle 7 the bne at 0x10 is in EX, and IF and ID hold 0x18 and 0x14 — the fall-through path, because
the counter was still weakly-not-taken. EX resolves the branch as taken, so in cycle 8 both of those are
gone (---------- is a bubble) and IF has restarted at the target, 0x08. Two instructions fetched,
decoded and thrown away: the 2-cycle penalty, once.

What a correct prediction looks like
The same run, a few cycles later, after update has pushed the counter to taken:
[cycle 11] IF 0x00000008 | ID 0x00000010 | EX 0x0000000c | MEM 0x00000008 | WB ------
----
[cycle 12] IF 0x0000000c | ID 0x00000008 | EX 0x00000010 | MEM 0x0000000c | WB

---

<!-- Page 5 -->

0x00000008
[cycle 13] IF 0x00000010 | ID 0x0000000c | EX 0x00000008 | MEM 0x00000010 | WB
0x0000000c
IF is now walking 0x08 → 0x0c → 0x10 → 0x08, straight around the loop with no bubble anywhere. In
cycle 12 the bne is in EX and the first instruction of the next iteration is already in ID. The branch costs
nothing. That is the entire point of the lab, and it is visible in three lines of trace.

5. Task 1 — The Interface, and the Static Predictors
File you write: pipeline/bp_static.c

The interface
#define BP_TABLE_BITS 6
#define BP_TABLE_SIZE (1 << BP_TABLE_BITS)                /* 64 entries */

typedef struct {
    uint8_t table[BP_TABLE_SIZE];
} BPState;

/* Branch PCs are word-aligned, so the low 2 bits carry no information. */
static inline int bp_index(uint32_t pc) {
    return (int)((pc >> 2) & (BP_TABLE_SIZE - 1));
}

typedef struct {
    const char *name;
    const char *description;
    void (*reset)(BPState *s);
    /* 1 = predict taken. `target` is the branch's taken address. */
    int (*predict)(const BPState *s, uint32_t pc, uint32_t target);
    /* Called in EX once the real outcome is known. */
    void (*update)(BPState *s, uint32_t pc, uint32_t target, int taken);
} BranchPredictor;
Three functions and one small array. Note what each one may touch:

    •   predict takes const BPState * — it reads state, it never writes it. A predictor that learns
        inside predict is predicting from a future it has not seen yet.
    •   update is the only place state changes, and it runs in EX, where taken is a fact rather than a
        guess.
    •   reset puts the table in its starting condition. It is called once at startup, and what "starting
        condition" means is a design decision you will have to defend.
    •   The table belongs to the pipeline, not to your predictor, so none of these functions may hold
        static variables of their own. Two predictors must be able to coexist in the same binary.

Always not taken
The simplest possible predictor, and the one your pipeline already behaved like before this lab:
#include "branch_predictor.h"
#include <string.h>

static void reset_none(BPState *s) {

---

<!-- Page 6 -->

    memset(s, 0, sizeof(*s));
}

static void update_none(BPState *s, uint32_t pc, uint32_t target, int taken) {
    (void)s; (void)pc; (void)target; (void)taken;
}

static int predict_not_taken(const BPState *s, uint32_t pc, uint32_t target) {
    (void)s; (void)pc; (void)target;
    return 0;
}

const BranchPredictor bp_not_taken = {
     "not-taken",
     "static: always predict not taken",
     reset_none,
     predict_not_taken,
     update_none,
};
The (void)x; casts silence -Wunused-parameter; the build must stay warning-free under -Wall -
Wextra.

Backward taken, forward not taken
Now one real idea, still with no state at all. Loops branch backwards and are taken on every iteration but
the last; forward branches usually jump over a rarely-executed block. So:
static int predict_btfn(const BPState *s, uint32_t pc, uint32_t target) {
    (void)s;
    return target < pc;
}

const BranchPredictor bp_btfn = {
    "btfn",
    "static: backward taken, forward not taken",
    reset_none,
    predict_btfn,
    update_none,
};
One comparison, no table, no training — and on the programs in this suite it is the most accurate of the
four. Keep that result in mind when you write the report: "dynamic" is not a synonym for "better".

Checkpoint. --predictor not-taken on t1_counted_loop.s gives 59 cycles and 9 mispredicts; --
predictor btfn gives 43 cycles and 1 mispredict. Both must leave the register file identical to --
single-cycle.


6. Task 2 — The 1-Bit Predictor
File you write: pipeline/bp_dynamic.c

A static predictor treats every execution of a branch the same way for ever. A dynamic one remembers.
The simplest thing worth remembering is one bit per branch: what did this branch do last time?

---

<!-- Page 7 -->

/* table[i] = 1 if the last branch that used entry i was taken. */

static void reset_1bit(BPState *s) {
    memset(s->table, 0, sizeof(s->table));               /* start: not taken */
}

static int predict_1bit(const BPState *s, uint32_t pc, uint32_t target) {
    (void)target;
    return s->table[bp_index(pc)];
}

static void update_1bit(BPState *s, uint32_t pc, uint32_t target, int taken) {
    (void)target;
    s->table[bp_index(pc)] = taken ? 1 : 0;
}

const BranchPredictor bp_1bit = {
     "1bit",
     "dynamic: 1-bit last-outcome table (64 entries)",
     reset_1bit,
     predict_1bit,
     update_1bit,
};
target is now unused: the direction is read out of the table, not computed from the addresses. That is
the trade — you gave up the one piece of free information btfn used, and bought the ability to learn.

Which entry, and what happens when two branches collide
bp_index(pc) is pc[7:2]: drop the two always-zero bits of a word-aligned address, keep the next six,
giving 64 entries. Two consequences your report has to address:

    •   A branch gets its own entry only if no other branch shares those six bits. Branches whose
        addresses differ by a multiple of 256 bytes alias to the same entry, and then they train each
        other — one loop's exit can make another loop mispredict.
    •   Nothing in the table says which branch wrote an entry. A real design would add a tag; this one
        does not, so aliasing is silent. None of the four supplied test programs is long enough to alias
        (the largest spans 148 bytes), which is exactly why aliasing is a thing you have to reason about
        rather than observe.

The 1-bit predictor's characteristic failure
On one long loop it is nearly perfect: 2 misses out of 10 on t1_counted_loop.s, the same as the 2-bit
version. Its weakness only appears when a loop is re-entered, which is what t2_nested_loops.s is built
from — an inner loop of 3 iterations run 4 times:
inner branch outcomes:         T T N    T T N    T T N    T T N
1-bit prediction:              N T T    N T T    N T T    N T T
                               ^   ^    ^   ^    ^   ^    ^   ^
Every exit is a miss (it predicted T, got N), and then every re-entry is a miss too (it predicted N, got T).
Two misses per pass, four passes: 8 misses on 12 executions. One bit of history cannot tell "this branch is
usually taken" from "this branch was taken last time", and for a loop those are different claims.

---

<!-- Page 8 -->

Checkpoint. --predictor 1bit gives 45 cycles and 2 mispredicts on t1_counted_loop.s, and 76
cycles with 10 mispredicts on t2_nested_loops.s — worse than btfn on both.

7. Task 3 — The 2-Bit Saturating Counter
Same file: pipeline/bp_dynamic.c

The 1-bit predictor changes its mind on a single surprise. The fix is hysteresis: give each entry a two-bit
counter, so one unusual outcome weakens a prediction without reversing it.
       not taken                                    taken
  0 <----------- 1 <----------- 2 <----------- 3
  strongly       weakly         weakly         strongly
  not taken      not taken      taken          taken
  |--- predict NOT TAKEN ---|   |--- predict TAKEN ---|
     (taken --> move right, not taken --> move left, both saturate)


#define CTR_WEAK_NOT_TAKEN 1
#define CTR_MAX 3

static void reset_2bit(BPState *s) {
    memset(s->table, CTR_WEAK_NOT_TAKEN, sizeof(s->table));
}

static int predict_2bit(const BPState *s, uint32_t pc, uint32_t target) {
    (void)target;
    return s->table[bp_index(pc)] >= 2;
}

static void update_2bit(BPState *s, uint32_t pc, uint32_t target, int taken) {
    (void)target;
    uint8_t *c = &s->table[bp_index(pc)];
    if (taken) { if (*c < CTR_MAX) (*c)++; }
    else       { if (*c > 0)       (*c)--; }
}

const BranchPredictor bp_2bit = {
    "2bit",
    "dynamic: 2-bit saturating counters (64 entries, start weakly not taken)",
    reset_2bit,
    predict_2bit,
    update_2bit,
};
Three details that are each worth a mark:

    •   Saturate, do not wrap. Without the < CTR_MAX and > 0 guards the counter rolls over from 3 to
        0, and a perfectly predictable loop mispredicts every fourth iteration. This is the most common
        bug in the task and the cycle counts expose it immediately.
    •   The threshold is the high bit. >= 2 is the same as testing bit 1, which is why this is called a 2-bit
        predictor: the top bit is the prediction and the bottom bit is the confidence.

---

<!-- Page 9 -->

    •   The starting value is a design choice. This reference starts at 1, weakly not taken, which costs
        one extra miss on the first execution of every loop but never predicts taken for a branch it has
        not yet seen. Starting at 2 would reverse that trade. Say which you chose and why.

Where it wins
Back to the nested loop. The inner branch's entry saturates at 3, so the exit only knocks it to 2 — still
predicting taken, so the re-entry is correct:
inner branch outcomes:          T T N    T T N    T T N    T T N
2-bit counter after:            2 3 2    3 3 2    3 3 2    3 3 2
2-bit prediction:               N T T    T T T    T T T    T T T
                                ^   ^        ^        ^        ^
Five misses instead of the 1-bit predictor's eight: the two warm-up misses, then one per loop exit. On
t2_nested_loops.s that is 7 total mispredicts against 10, and 70 cycles against 76.

Where neither wins
t3_alternating.s contains a forward branch whose outcome is T N T N T N T N. The 1-bit predictor
always guesses the previous outcome, so it is wrong every single time — 8 misses out of 8. The 2-bit
counter bounces between weakly-not-taken and weakly-taken and is also wrong every time. Both score
10 total mispredicts on that program, where btfn scores 5 — a static rule beating both dynamic ones,
badly.

This is not a flaw in your code. A predictor that keys on nothing but one branch's own recent history
cannot represent an alternating pattern; you would need correlating history (the outcomes of other
recent branches) to catch it. That is the next predictor up, and Section 12 asks you to design it.

Checkpoint. --predictor 2bit gives 45 cycles on t1_counted_loop.s, 70 on t2_nested_loops.s,
and 74 on t3_alternating.s.

8. Task 4 — Measuring It
The pipeline counts everything for you. A --batch run ends with:
pipeline            = 5-stage, hazard unit 'forward', predictor '2bit'
retired             = 37
CPI                 = 1.216
stalls              = 0
forwards            = 12
branches            = 10        conditional branches executed
mispredicts         = 2         ... of which the guess was wrong
accuracy            = 80.0%
jal                 = 0         redirected in IF: free
jalr                = 0         never predicted: 2 cycles each
redirects           = 2         mispredicts + jalr
squashed            = 4         instructions thrown away (2 per redirect)
Nothing here needs new code. The task is to turn these numbers into an account of where the cycles
went, for every program and every predictor, and to check two identities on each run:
\text{cycles} = \text{retired} + 4 + 2 \times \text{redirects} + \text{stalls}

---

<!-- Page 10 -->

\text{redirects} = \text{mispredicts} + \text{jalr}
The first says every cycle is accounted for. The second says exactly which part of the loss a predictor can
attack: the jalr term is beyond its reach, and jal does not appear at all.

The mispredict budget
For each program, work out the best score any direction predictor could possibly get, and compare your
four against it:

Quantity                                 Where it comes from

control cycles lost                      2 x redirects

of which unavoidable here                2 x jalr — no return-address stack in this design

of which addressable                     2 x mispredicts

best possible                            2 cycles per unpredictable branch; for a loop executed once,
                                         that is its warm-up miss plus its exit

On t1_counted_loop.s the floor is 1 miss — the exit of a loop whose body runs nine times, which no
history can foresee. btfn reaches that floor. Your dynamic predictors do not, because they spend a miss
learning something btfn knew from the address arithmetic.

Report for each program and predictor: branches, mispredicts, accuracy, redirects, cycles and CPI,
plus the single sentence that explains the gap between that predictor's miss count and the floor.

9. Verification

Step 1 — the register files must match
A predictor changes when instructions are fetched, never what they compute. A wrong guess is
squashed before it can write anything, so all four predictors must leave the register file identical to the
single-cycle oracle:
for f in tests/*.s; do
  ./rvsim $f --batch --single-cycle > ref.txt
  for p in not-taken btfn 1bit 2bit; do
     ./rvsim $f --batch --predictor $p > got.txt
     diff <(grep '^x' ref.txt) <(grep '^x' got.txt) >/dev/null || echo "$f $p
MISMATCH"
  done
done
Or just make test, which checks this and the golden statistics for every program under every predictor
— 40 checks, RARS cross-checks included. A register mismatch means a squash is leaking, not that your
prediction is poor.

Step 2 — the numbers must be explainable
All four programs, all four predictors. Instrs is the retired count, identical everywhere. Lab 2 is the
same program run on the previous lab's pipeline, for comparison.

---

<!-- Page 11 -->

Program                     Instrs    Lab 2        not-taken        btfn        1bit    2bit    single-cycle
t1_counted_loop.s           37        59           59               43          45      45      37
t2_nested_loops.s           52        78           78               66          76      70      52
t3_alternating.s            50        84           76               64          74      74      50
t4_calls.s                  219       344          278              278         280     278     219

total cycles                358       565          491              451         475     467     358

CPI                         1.000     1.578        1.372            1.260       1.327   1.305   1.000

And the mispredict counts those cycle numbers come from:

Program                      Branches         not-taken        btfn         1bit        2bit    jal      jalr

t1_counted_loop.s            10               9                1            2           2       0        0
t2_nested_loops.s            16               11               5            10          7       0        0
t3_alternating.s             16               11               5            10          10      4        0
t4_calls.s                   20               2                2            3           2       33       18

total                        62               33               13           25          21      37       18

accuracy                                      46.8%            79.0%        59.7%       66.1%

Three results in these tables are worth more than the rest, and the report asks about each:

      •   t4_calls.s drops from 344 to 278 before any prediction happens. not-taken is the same
          policy the previous lab used, yet it saves 66 cycles — exactly 2 × 33 jal. That is the IF pre-
          decode, not the predictor.
      •   btfn wins overall, with no state at all, on this suite of short programs. Dynamic predictors pay a
          warm-up miss per branch and these programs are too short to amortise it.
      •   2bit beats 1bit on t2 (7 vs 10) and ties it on t3 (10 vs 10). The first is hysteresis paying off on
          loop re-entry; the second is both predictors failing on a pattern neither can represent.

What must match, and what may not
retired must equal the single-cycle cycle count, and every register and printed character must match
the oracle, under all four predictors. cycles, branches, mispredicts, redirects and jal/jalr must
match the tables.

One defensible deviation: if you start the 2-bit counters at 2 (weakly taken) instead of 1, 2bit loses its
first-execution miss on backward branches and gains one on forward branches — t1 improves to 1 miss,
t3 gets worse. If you do that, say so in the report and give both sets of figures. Any other difference is a
bug.
