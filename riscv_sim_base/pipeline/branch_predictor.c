#include "branch_predictor.h"
#include <string.h>

static const BranchPredictor *predictors[] = {
    &bp_not_taken,
    &bp_btfn,
    &bp_1bit,
    &bp_2bit,
    NULL
};

const BranchPredictor *bp_find(const char *name) {
    if (!name) return &bp_2bit; /* default */
    for (int i = 0; predictors[i]; i++) {
        if (strcmp(predictors[i]->name, name) == 0) {
            return predictors[i];
        }
    }
    return NULL;
}

