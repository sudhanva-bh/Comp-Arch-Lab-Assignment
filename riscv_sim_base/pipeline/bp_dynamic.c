#include "branch_predictor.h"
#include <string.h>

/* -------------------------------------------------------------
 * 1-Bit Dynamic Predictor (Task 2)
 * ------------------------------------------------------------- */
static void reset_1bit(BPState *s) {
    memset(s->table, 0, sizeof(s->table)); /* start: not taken */
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

/* -------------------------------------------------------------
 * 2-Bit Saturating Counter Predictor (Task 3)
 * ------------------------------------------------------------- */
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
    if (taken) {
        if (*c < CTR_MAX) (*c)++;
    } else {
        if (*c > 0) (*c)--;
    }
}

const BranchPredictor bp_2bit = {
    "2bit",
    "dynamic: 2-bit saturating counters (64 entries, start weakly not taken)",
    reset_2bit,
    predict_2bit,
    update_2bit,
};

