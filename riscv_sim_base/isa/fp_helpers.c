#include "fp_helpers.h"
#include "platform.h"
#include "softfloat.h"
#include <stdio.h>
int is_nan_bits(uint32_t a) { return (a & 0x7fffffffu)>0x7f800000u; }
int is_snan_bits(uint32_t a) { return is_nan_bits(a) && !(a & 0x400000u); }
void fp_illegal(CPUState *c, const char *message) {
    c->status=CPU_ILLEGAL_INSTRUCTION; c->fault_addr=c->pc;
    snprintf(c->fault_msg,sizeof(c->fault_msg),"%s at pc=0x%08x",message,c->pc);
}
int resolve_rm(CPUState *c, const DecodedInstr *i) {
    int rm=i->funct3==7 ? (int)cpu_frm(c) : (int)i->funct3;
    if (rm>4) { fp_illegal(c,"illegal floating-point rounding mode"); return -1; }
    return rm;
}
/* Save/restore TLS numerical state so no simulator instance inherits another
   operation's flags or rounding. SoftFloat uses the same flag/rm bit values. */
typedef struct { uint_fast8_t rm, flags, tiny; } FPEnv;
static FPEnv begin(int rm) {
    FPEnv e={softfloat_roundingMode,softfloat_exceptionFlags,softfloat_detectTininess};
    softfloat_roundingMode=(uint_fast8_t)rm; softfloat_exceptionFlags=0;
    softfloat_detectTininess=softfloat_tininess_afterRounding; return e;
}
static void end(FPEnv e, uint32_t *flags) {
    *flags |= softfloat_exceptionFlags;
    softfloat_roundingMode=e.rm; softfloat_exceptionFlags=e.flags; softfloat_detectTininess=e.tiny;
}
uint32_t fp_arith(ArithOp op,uint32_t a,uint32_t b,uint32_t d,int rm,uint32_t *flags) {
    FPEnv e=begin(rm); float32_t A={a},B={b},D={d},R={0};
    switch(op) {
    case AR_ADD:R=f32_add(A,B);break;
    case AR_SUB:R=f32_sub(A,B);break;
    case AR_MUL:R=f32_mul(A,B);break;
    case AR_DIV:R=f32_div(A,B);break;
    case AR_SQRT:R=f32_sqrt(A);break;
    case AR_FMA:R=f32_mulAdd(A,B,D);break;
    }
    end(e,flags); return R.v;
}
uint32_t fp_to_int(uint32_t a,int u,int rm,uint32_t *flags) {
    FPEnv e=begin(rm); float32_t A={a};
    uint32_t r=u ? (uint32_t)f32_to_ui32(A,(uint_fast8_t)rm,true) :
                  (uint32_t)f32_to_i32(A,(uint_fast8_t)rm,true);
    end(e,flags);return r;
}
uint32_t int_to_fp(uint32_t a,int u,int rm,uint32_t *flags) {
    FPEnv e=begin(rm); float32_t r=u ? ui32_to_f32(a) : i32_to_f32((int32_t)a);
    end(e,flags);return r.v;
}
int fp_compare(uint32_t a,uint32_t b,int f3,uint32_t *flags) {
    FPEnv e=begin(0); float32_t A={a},B={b};
    int r=f3==2 ? f32_eq(A,B) : f3==1 ? f32_lt(A,B) : f32_le(A,B);
    end(e,flags);return r;
}
