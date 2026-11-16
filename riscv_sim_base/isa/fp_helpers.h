#ifndef FP_HELPERS_H
#define FP_HELPERS_H
#include "isa_registry.h"
#define CANONICAL_NAN 0x7fc00000u
#define SIGN_BIT 0x80000000u
typedef enum { AR_ADD, AR_SUB, AR_MUL, AR_DIV, AR_SQRT, AR_FMA } ArithOp;
int is_nan_bits(uint32_t b);
int is_snan_bits(uint32_t b);
int resolve_rm(CPUState *c, const DecodedInstr *i);
void fp_illegal(CPUState *c, const char *message);
uint32_t fp_arith(ArithOp op, uint32_t a, uint32_t b, uint32_t d, int rm, uint32_t *flags);
uint32_t fp_to_int(uint32_t a, int is_unsigned, int rm, uint32_t *flags);
uint32_t int_to_fp(uint32_t a, int is_unsigned, int rm, uint32_t *flags);
int fp_compare(uint32_t a, uint32_t b, int funct3, uint32_t *flags);
#endif
