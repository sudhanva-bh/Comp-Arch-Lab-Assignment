#ifndef PIPELINE_HAZARD_H
#define PIPELINE_HAZARD_H
#include "pipeline_state.h"
typedef struct { int stall, load_use; } HazardVerdict;
typedef struct { uint32_t a, b; int fwd_ex, fwd_mem; } Operands;
int produces(int valid, const ControlSignals *c, uint32_t rd, uint32_t rs);
HazardVerdict hazard_check(const Pipeline *p, const DecodedInstr *d, const ControlSignals *c);
Operands forward_operands(const Pipeline *p, const IDEX *d);
#endif
