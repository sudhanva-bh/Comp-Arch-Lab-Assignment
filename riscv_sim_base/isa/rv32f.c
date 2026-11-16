#include "rv32f.h"
#include "fp_helpers.h"
#include <stdio.h>
#define FR(n) cpu_read_freg(c,(int)(n))
static void flw(CPUState *c,Memory *m,DecodedInstr *i) {
    uint32_t a=cpu_read_reg(c,i->rs1)+(uint32_t)i->imm,v;
    if (memory_read_word(m,a,&v)!=MEM_OK) {
        c->status=CPU_MEM_FAULT;c->fault_addr=a;
        snprintf(c->fault_msg,sizeof(c->fault_msg),"flw out of bounds at 0x%08x",a);return;
    }
    cpu_write_freg(c,i->rd,v);
}
static void fsw(CPUState *c,Memory *m,DecodedInstr *i) {
    uint32_t a=cpu_read_reg(c,i->rs1)+(uint32_t)i->imm;
    if (memory_write_word(m,a,FR(i->rs2))!=MEM_OK) {
        c->status=CPU_MEM_FAULT;c->fault_addr=a;
        snprintf(c->fault_msg,sizeof(c->fault_msg),"fsw out of bounds at 0x%08x",a);
    }
    c->fp_used=1;
}
static void arith(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m; int rm=resolve_rm(c,i);if(rm<0)return;
    uint32_t a=FR(i->rs1),b=FR(i->rs2),d=FR(i->rs3),flags=0;
    ArithOp op=AR_ADD;
    if(i->opcode!=0x53) {
        op=AR_FMA;
        if(i->opcode==0x47 || i->opcode==0x4f)d^=SIGN_BIT;
        if(i->opcode==0x4b || i->opcode==0x4f)a^=SIGN_BIT;
    } else switch(i->funct7) {
        case 0:op=AR_ADD;break;case 4:op=AR_SUB;break;case 8:op=AR_MUL;break;
        case 12:op=AR_DIV;break;case 44:op=AR_SQRT;break;
    }
    uint32_t r=fp_arith(op,a,b,d,rm,&flags);
    cpu_raise_fflags(c,flags);cpu_write_freg(c,i->rd,r);
}
static void sign_inject(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;uint32_t a=FR(i->rs1),b=FR(i->rs2),s=b&SIGN_BIT;
    if(i->funct3==1)s^=SIGN_BIT;
    if(i->funct3==2)s^=a&SIGN_BIT;
    cpu_write_freg(c,i->rd,(a&~SIGN_BIT)|s);
}
static void move(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;c->fp_used=1;
    if(i->funct7==0x78)cpu_write_freg(c,i->rd,cpu_read_reg(c,i->rs1));
    else cpu_write_reg(c,i->rd,FR(i->rs1));
}
static void classify(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;uint32_t a=FR(i->rs1),e=(a>>23)&255,f=a&0x7fffff;int s=a>>31,bit;
    if(e==255)bit=f ? (is_snan_bits(a)?8:9) : (s?0:7);
    else if(e==0)bit=f ? (s?2:5) : (s?3:4);
    else bit=s?1:6;
    cpu_write_reg(c,i->rd,1u<<bit);c->fp_used=1;
}
static void compare(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;uint32_t flags=0;int r=fp_compare(FR(i->rs1),FR(i->rs2),i->funct3,&flags);
    cpu_raise_fflags(c,flags);cpu_write_reg(c,i->rd,(uint32_t)r);
}
static void minmax(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;uint32_t a=FR(i->rs1),b=FR(i->rs2),r,flags=0;
    if(is_snan_bits(a)||is_snan_bits(b))flags|=FFLAG_NV;
    if(is_nan_bits(a)&&is_nan_bits(b))r=CANONICAL_NAN;
    else if(is_nan_bits(a))r=b;
    else if(is_nan_bits(b))r=a;
    else if(!(a&~SIGN_BIT)&&!(b&~SIGN_BIT))r=i->funct3 ? a&b : a|b;
    else { uint32_t ignored=0;int less=fp_compare(a,b,1,&ignored);
        r=i->funct3 ? (less?b:a) : (less?a:b); }
    cpu_raise_fflags(c,flags);cpu_write_freg(c,i->rd,r);
}
static void convert(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;int rm=resolve_rm(c,i);if(rm<0)return;
    uint32_t flags=0,r;
    if(i->funct7==0x60) {
        r=fp_to_int(FR(i->rs1),i->rs2==1,rm,&flags);cpu_write_reg(c,i->rd,r);
    } else {
        r=int_to_fp(cpu_read_reg(c,i->rs1),i->rs2==1,rm,&flags);cpu_write_freg(c,i->rd,r);
    }
    cpu_raise_fflags(c,flags);
}
static void reg(const char *name,unsigned op,int f3,int f7,InstrFormat fmt,ExecuteFn fn,unsigned flags,unsigned sel) {
    register_instruction_ex((InstructionHandler){name,op,f3,f7,fmt,fn},flags|IF_EXEC_HANDLER|IF_FP_STATE,sel);
}
void register_rv32f(void) {
    unsigned fff=IF_RD_FP|IF_RS1_FP|IF_RS2_FP;
    reg("flw",0x07,2,-1,FMT_I_LOAD,flw,IF_RD_FP,0);
    reg("fsw",0x27,2,-1,FMT_S,fsw,IF_RS2_FP,0);
    reg("fsgnj.s",0x53,0,0x10,FMT_R,sign_inject,fff,0);
    reg("fsgnjn.s",0x53,1,0x10,FMT_R,sign_inject,fff,0);
    reg("fsgnjx.s",0x53,2,0x10,FMT_R,sign_inject,fff,0);
    reg("fmv.x.w",0x53,0,0x70,FMT_R2,move,IF_RS1_FP|IF_RS2_FIXED,0);
    reg("fmv.w.x",0x53,0,0x78,FMT_R2,move,IF_RD_FP|IF_RS2_FIXED,0);
    reg("fclass.s",0x53,1,0x70,FMT_R2,classify,IF_RS1_FP|IF_RS2_FIXED,0);
    const char *names[]={"fadd.s","fsub.s","fmul.s","fdiv.s"};
    for(int n=0;n<4;n++)reg(names[n],0x53,-1,n*4,FMT_R,arith,fff|IF_HAS_RM,0);
    reg("fsqrt.s",0x53,-1,0x2c,FMT_R2,arith,IF_RD_FP|IF_RS1_FP|IF_HAS_RM|IF_RS2_FIXED,0);
    const char *fma[]={"fmadd.s","fmsub.s","fnmsub.s","fnmadd.s"};
    for(int n=0;n<4;n++)reg(fma[n],0x43+4*n,-1,0,FMT_R4,arith,fff|IF_RS3_FP|IF_HAS_RM,0);
    reg("feq.s",0x53,2,0x50,FMT_R,compare,IF_RS1_FP|IF_RS2_FP,0);
    reg("flt.s",0x53,1,0x50,FMT_R,compare,IF_RS1_FP|IF_RS2_FP,0);
    reg("fle.s",0x53,0,0x50,FMT_R,compare,IF_RS1_FP|IF_RS2_FP,0);
    reg("fmin.s",0x53,0,0x14,FMT_R,minmax,fff,0);
    reg("fmax.s",0x53,1,0x14,FMT_R,minmax,fff,0);
    reg("fcvt.w.s",0x53,-1,0x60,FMT_R2,convert,IF_RS1_FP|IF_HAS_RM|IF_RS2_FIXED,0);
    reg("fcvt.wu.s",0x53,-1,0x60,FMT_R2,convert,IF_RS1_FP|IF_HAS_RM|IF_RS2_FIXED,1);
    reg("fcvt.s.w",0x53,-1,0x68,FMT_R2,convert,IF_RD_FP|IF_HAS_RM|IF_RS2_FIXED,0);
    reg("fcvt.s.wu",0x53,-1,0x68,FMT_R2,convert,IF_RD_FP|IF_HAS_RM|IF_RS2_FIXED,1);
}
