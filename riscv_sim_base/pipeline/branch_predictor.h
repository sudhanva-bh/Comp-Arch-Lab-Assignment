#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H
#include <stdint.h>
#define BP_TABLE_BITS 6
#define BP_TABLE_SIZE (1u << BP_TABLE_BITS)
typedef struct { uint8_t table[BP_TABLE_SIZE]; } BPState;
static inline int bp_index(uint32_t pc) {
    return (int)((pc >> 2) & (BP_TABLE_SIZE - 1));
}
typedef struct {
    const char *name, *description;
    void (*reset)(BPState *s);
    int (*predict)(const BPState *s, uint32_t pc, uint32_t target);
    void (*update)(BPState *s, uint32_t pc, uint32_t target, int taken);
} BranchPredictor;
extern const BranchPredictor bp_not_taken, bp_btfn, bp_1bit, bp_2bit;
const BranchPredictor *bp_find(const char *name);
#endif
