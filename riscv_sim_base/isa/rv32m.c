#include "rv32m.h"
#include "isa_registry.h"
#include <limits.h>
static void execute(CPUState *c,Memory *m,DecodedInstr *i) {
    (void)m;uint32_t a=cpu_read_reg(c,i->rs1),b=cpu_read_reg(c,i->rs2),v=0;
    int32_t x=(int32_t)a,y=(int32_t)b;
    switch(i->funct3) {
    case 0:v=(uint32_t)((uint64_t)a*b);break;
    case 1:v=(uint32_t)((uint64_t)((int64_t)x*y)>>32);break;
    case 2:v=(uint32_t)((uint64_t)((int64_t)x*(int64_t)(uint64_t)b)>>32);break;
    case 3:v=(uint32_t)(((uint64_t)a*b)>>32);break;
    case 4:v=!b?UINT32_MAX:x==INT32_MIN&&y==-1?a:(uint32_t)(x/y);break;
    case 5:v=!b?UINT32_MAX:a/b;break;
    case 6:v=!b?a:x==INT32_MIN&&y==-1?0:(uint32_t)(x%y);break;
    case 7:v=!b?a:a%b;break;
    }
    cpu_write_reg(c,i->rd,v);
}
void register_rv32m(void) {
    const char *names[]={"mul","mulh","mulhsu","mulhu","div","divu","rem","remu"};
    for(int n=0;n<8;n++)register_instruction_ex((InstructionHandler){names[n],0x33,n,1,FMT_R,execute},IF_EXEC_HANDLER,0);
}
