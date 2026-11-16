#include "fp_helpers.h"
#include "rv32f.h"
#include "zicsr.h"
#include "rv32m.h"
#include "decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void vector(ArithOp op,uint32_t a,uint32_t b,uint32_t c,int rm,uint32_t value,uint32_t flags) {
    uint32_t got=0,r=fp_arith(op,a,b,c,rm,&got);assert(r==value);assert(got==flags);
}
static void instruction(CPUState *c,const char *name,unsigned rd,unsigned s1,unsigned s2,unsigned s3,unsigned rm) {
    const InstructionHandler *h=isa_find_by_mnemonic(name);assert(h);
    DecodedInstr i={0};i.opcode=h->opcode;i.funct3=(isa_flags(h)&IF_HAS_RM)?rm:(uint32_t)h->funct3;
    i.funct7=h->funct7<0?0:(uint32_t)h->funct7;
    i.rd=rd;i.rs1=s1;i.rs2=(isa_flags(h)&IF_RS2_FIXED)?isa_rs2_sel(h):s2;i.rs3=s3;
    h->execute(c,NULL,&i);
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--vectors")) {
        int op,rm;unsigned a,b,c;
        while(scanf("%d %d %x %x %x",&op,&rm,&a,&b,&c)==5) {
            uint32_t flags=0,r;
            if(op<6)r=fp_arith((ArithOp)op,a,b,c,rm,&flags);
            else if(op<8)r=fp_to_int(a,op==7,rm,&flags);
            else r=int_to_fp(a,op==9,rm,&flags);
            printf("%08x %02x\n",r,flags);
        }
        return 0;
    }
    vector(AR_DIV,0,0,0,0,CANONICAL_NAN,16);
    vector(AR_DIV,0x3f800000,0,0,0,0x7f800000,8);
    vector(AR_DIV,0x7f800000,0,0,0,0x7f800000,0);
    vector(AR_ADD,0x7f7fffff,0x7f7fffff,0,0,0x7f800000,5);
    vector(AR_ADD,0x7f7fffff,0x7f7fffff,0,1,0x7f7fffff,5);
    vector(AR_MUL,0x00800000,0x00800000,0,0,0,3);
    vector(AR_MUL,1,0x3f800000,0,0,1,0);
    vector(AR_SUB,0x7f800000,0x7f800000,0,0,CANONICAL_NAN,16);
    vector(AR_ADD,0x7fc12345,0x3f800000,0,0,CANONICAL_NAN,0);
    vector(AR_ADD,0x7fa12345,0x3f800000,0,0,CANONICAL_NAN,16);
    vector(AR_SQRT,0xbf800000,0,0,0,CANONICAL_NAN,16);
    vector(AR_SQRT,0x80000000,0,0,0,0x80000000,0);
    vector(AR_SQRT,0x40800000,0,0,0,0x40000000,0);
    vector(AR_SQRT,0x40000000,0,0,0,0x3fb504f3,1);
    vector(AR_FMA,0x7f800000,0,0x7fc00000,0,CANONICAL_NAN,16);
    /* Product differs from one by -2^-46; separately rounded multiply loses it. */
    vector(AR_FMA,0x3f800001,0x3f7ffffe,0xbf800000,0,0xa8800000,0);
    for(int rm=0;rm<5;rm++) {
        vector(AR_ADD,0x3f800000,0x33800000,0,rm,
               rm==3||rm==4?0x3f800001:0x3f800000,1);
        vector(AR_ADD,0xbf800000,0xb3800000,0,rm,
               rm==2||rm==4?0xbf800001:0xbf800000,1);
        vector(AR_ADD,0x3f800000,0xbf800000,0,rm,rm==2?0x80000000:0,0);
        uint32_t flags=0;
        unsigned pos[]={2,2,2,3,3},neg[]={0xfffffffe,0xfffffffe,0xfffffffd,0xfffffffe,0xfffffffd};
        assert(fp_to_int(0x40200000,0,rm,&flags)==pos[rm] && flags==1);
        flags=0;assert(fp_to_int(0xc0200000,0,rm,&flags)==neg[rm] && flags==1);
        flags=0;assert(fp_to_int(CANONICAL_NAN,0,rm,&flags)==0x7fffffff && flags==16);
        flags=0;assert(fp_to_int(0xc0200000,1,rm,&flags)==0 && flags==16);
        flags=0;assert(int_to_fp(0x1000001,0,rm,&flags)==(rm==3||rm==4?0x4b800001:0x4b800000) && flags==1);
    }
    isa_registry_init();register_rv32m();register_rv32f();register_zicsr();
    CPUState c;cpu_reset(&c,0);c.fregs[1]=0x80000000;c.fregs[2]=0;
    instruction(&c,"fmin.s",0,1,2,0,0);assert(c.fregs[0]==0x80000000);
    instruction(&c,"fmax.s",0,1,2,0,0);assert(c.fregs[0]==0);
    c.fregs[1]=0x7fa00000;c.fregs[2]=0x3f800000;
    instruction(&c,"fsgnjn.s",0,1,1,0,0);assert(c.fregs[0]==0xffa00000 && !c.fcsr);
    instruction(&c,"fmin.s",0,1,2,0,0);assert(c.fregs[0]==0x3f800000 && c.fcsr==16);
    c.fregs[1]=CANONICAL_NAN;c.fcsr=0;
    instruction(&c,"feq.s",3,1,1,0,0);assert(c.regs[3]==0 && !c.fcsr);
    instruction(&c,"fle.s",3,1,2,0,0);assert(c.regs[3]==0 && c.fcsr==16);
    unsigned classes[]={0xff800000,0xbf800000,0x80000001,0x80000000,0,1,0x3f800000,0x7f800000,0x7fa00000,0x7fc00000};
    for(int n=0;n<10;n++){c.fregs[1]=classes[n];instruction(&c,"fclass.s",3,1,0,0,0);assert(c.regs[3]==(1u<<n));}
    for(int rm=5;rm<8;rm++) {
        cpu_reset(&c,0);c.fcsr=rm==7?0xe0:0;c.fregs[0]=123;
        instruction(&c,"fadd.s",0,1,2,0,rm);
        assert(c.status==CPU_ILLEGAL_INSTRUCTION && c.fregs[0]==123 && c.fcsr==(rm==7?0xe0u:0u));
    }
    /* Fixed-rs2 and unsupported R4 precision cannot accidentally decode. */
    DecodedInstr d;const InstructionHandler *h;
    assert(decode_instruction((0x60u<<25)|(2<<20)|0x53,&d,&h)!=DECODE_OK);
    assert(decode_instruction((1u<<25)|0x43,&d,&h)!=DECODE_OK);
    cpu_reset(&c,0);c.regs[1]=0x80000000;c.regs[2]=0xffffffff;
    instruction(&c,"div",3,1,2,0,0);assert(c.regs[3]==0x80000000);
    instruction(&c,"rem",3,1,2,0,0);assert(c.regs[3]==0);
    c.regs[2]=0;instruction(&c,"divu",3,1,2,0,0);assert(c.regs[3]==0xffffffff);
    instruction(&c,"remu",3,1,2,0,0);assert(c.regs[3]==0x80000000);
    puts("PASS: FP/RV32M directed numerical and encoding assertions");
    return 0;
}
