#include "zicsr.h"
#include "fp_helpers.h"
static void csr(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;
    if(i->csr<1 || i->csr>3) { fp_illegal(c,"unsupported CSR");return; }
    unsigned shift=i->csr==2?5:0,mask=i->csr==1?31:i->csr==2?7:255;
    uint32_t old=(c->fcsr>>shift)&mask;
    uint32_t src=i->funct3&4 ? i->rs1 : cpu_read_reg(c,i->rs1),v=old;
    switch(i->funct3&3) { case 1:v=src;break;case 2:v=old|src;break;case 3:v=old&~src;break; }
    c->fcsr=(c->fcsr&~(mask<<shift))|((v&mask)<<shift);
    cpu_write_reg(c,i->rd,old);c->fp_used=1;
}
void register_zicsr(void) {
    const char *n[]={"csrrw","csrrs","csrrc","csrrwi","csrrsi","csrrci"};
    for(int k=0;k<6;k++)register_instruction_ex((InstructionHandler){n[k],0x73,k<3?k+1:k+2,-1,FMT_CSR,csr},IF_EXEC_HANDLER|IF_FP_STATE,0);
}
