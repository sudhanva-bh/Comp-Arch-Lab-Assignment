#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H

#include <stdint.h>

#define BP_TABLE_BITS 6
#define BP_TABLE_SIZE (1 << BP_TABLE_BITS) /* 64 entries */

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

extern const BranchPredictor bp_not_taken;
extern const BranchPredictor bp_btfn;
extern const BranchPredictor bp_1bit;
extern const BranchPredictor bp_2bit;

const BranchPredictor *bp_find(const char *name);

#endif /* BRANCH_PREDICTOR_H */

