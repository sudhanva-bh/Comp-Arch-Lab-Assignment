#include "branch_predictor.h"
#include <string.h>
const BranchPredictor *bp_find(const char *name) {
    const BranchPredictor *all[] = {&bp_not_taken, &bp_btfn, &bp_1bit, &bp_2bit};
    if (!name) return NULL;
    for (unsigned i = 0; i < sizeof(all)/sizeof(all[0]); i++)
        if (strcmp(name, all[i]->name) == 0) return all[i];
    return NULL;
}
