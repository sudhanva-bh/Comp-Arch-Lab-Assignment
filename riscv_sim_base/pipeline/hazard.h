#ifndef HAZARD_H
#define HAZARD_H

#include "pipeline_state.h"

typedef struct {
    int stall;      /* hold ID, bubble EX, freeze IF                 */
    int load_use;   /* the producer is a load still in EX (Task 3)   */
} HazardVerdict;

typedef struct {
    uint32_t a, b;        /* the operands EX should actually use */
    int fwd_ex, fwd_mem;  /* how many operands came from each path (stats) */
} Operands;

int           produces(int valid, const ControlSignals *lc, uint32_t rd, uint32_t rs);
HazardVerdict hazard_check(const Pipeline *p, const DecodedInstr *d, const ControlSignals *c);
Operands      forward_operands(const Pipeline *p, const IDEX *d);

#endif /* HAZARD_H */
