#include "branch_predictor.h"
#include <string.h>

static void reset_none(BPState *s) {
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

