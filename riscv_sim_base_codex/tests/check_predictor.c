#include "pipeline_state.h"
#include <assert.h>
#include <string.h>
int main(void) {
    assert(!bp_find("unknown") && !bp_find(NULL));
    assert(bp_index(0x6c) == bp_index(0x16c));
    assert(bp_index(0x6c) != bp_index(0x170));
    const BranchPredictor *all[] = {&bp_not_taken, &bp_btfn, &bp_1bit, &bp_2bit};
    for (unsigned n = 0; n < 4; n++) {
        const BranchPredictor *b = all[n];
        assert(bp_find(b->name) == b);
        BPState s; memset(&s, 255, sizeof(s)); b->reset(&s);
        for (unsigned i = 0; i < BP_TABLE_SIZE; i++) assert(s.table[i] == (n == 3));
        BPState before = s;
        b->predict(&s, 64, 32); assert(!memcmp(&s, &before, sizeof(s)));
        if (n < 2) {
            assert(b->predict(&s, 64, 32) == (n == 1));
            assert(!b->predict(&s, 64, 64));
            assert(!b->predict(&s, 64, 96));
            b->update(&s, 64, 32, 1); assert(!memcmp(&s, &before, sizeof(s)));
        } else {
            for (int state = 0; state < (n == 3 ? 4 : 2); state++) {
                for (int taken = 0; taken < 2; taken++) {
                    b->reset(&s); s.table[bp_index(64)] = (uint8_t)state;
                    assert(b->predict(&s, 64, 0) == (n == 3 ? state >= 2 : state));
                    b->update(&s, 64, 0, taken);
                    int next = n == 2 ? taken : taken ? (state < 3 ? state+1 : 3) : (state > 0 ? state-1 : 0);
                    assert(s.table[bp_index(64)] == next);
                }
            }
            b->reset(&s); b->update(&s, 0x6c, 0, 1);
            assert(b->predict(&s, 0x16c, 0)); /* deliberate untagged alias */
        }
    }
    Pipeline a, b;
    pipeline_init(&a, 1); pipeline_init(&b, 1);
    pipeline_set_predictor(&a, &bp_2bit); pipeline_set_predictor(&b, &bp_1bit);
    bp_2bit.update(&a.bp, 64, 0, 1);
    assert(bp_2bit.predict(&a.bp, 64, 0)); assert(!bp_1bit.predict(&b.bp, 64, 0));
    pipeline_set_predictor(&a, &bp_2bit); assert(!bp_2bit.predict(&a.bp, 64, 0));
    return 0;
}
