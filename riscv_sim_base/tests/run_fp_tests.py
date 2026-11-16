#!/usr/bin/env python3
"""FP goldens, independent exact-rational rounding and precise-state checks."""
from pathlib import Path
from fractions import Fraction
import random
import re
import subprocess
ROOT=Path(__file__).resolve().parent.parent
checks=0

def need(ok,msg):
    global checks
    checks+=1
    if not ok: raise AssertionError(msg)

def run(path,*args):
    r=subprocess.run([str(ROOT/'rvsim'),str(path),*args],capture_output=True,text=True,check=True,timeout=10)
    need(not r.stderr,f'{path}: {r.stderr}')
    return r.stdout

def regs(s):return re.findall(r'[xf]\d+\s*/\w+\s*=0x[0-9a-f]+',s)
def fcsr(s):return re.search(r'fcsr =0x([0-9a-f]+)',s)[1]
def status(s):return re.search(r'status = (\w+)\s+cycles = (\d+)',s).groups()
def prints(s):return s.split('\n',1)[1].split('---------------------------- registers')[0]
def stats(s):return {k:float(v) if '.' in v else int(v) for k,v in re.findall(r'(\w+)=([0-9.]+)',s.split('pipeline:')[1])}

for p in sorted((ROOT/'tests/given/lab5_fp').glob('*.s')):
    oracle=run(p,'--single-cycle')
    for mode in ['single-cycle','forward','stall']:
        got=oracle if mode=='single-cycle' else run(p,'--batch','--hazard',mode)
        need(regs(got)==regs(oracle) and fcsr(got)==fcsr(oracle) and prints(got)==prints(oracle),f'{p} {mode}: oracle values/flags/output')
        gold=p.parent/'expected'/f'{p.stem}.{mode}.out'
        if gold.exists():
            expected=gold.read_text()
            need(regs(got)==regs(expected) and fcsr(got)==fcsr(expected),f'{p} {mode}: golden values/flags')
            need(status(got)==status(expected) and prints(got)==prints(expected),f'{p} {mode}: golden cycles/output')
            if mode!='single-cycle':
                st=stats(got);st['forwards']=st['fwd_ex']+st['fwd_mem']
                for key in ['retired','CPI','stalls','forwards','branches','mispredicts','accuracy','jal','jalr','redirects','squashed']:
                    match=re.search(r'^'+key+r'\s*=\s*([0-9.]+)',expected,re.M)
                    if match:need(st[key]==float(match[1]),f'{p} {mode}: {key}')
    subprocess.run([str(ROOT/'tests/pipeline_checks'),str(p)],check=True,stdout=subprocess.DEVNULL,timeout=20)
    need(True,f'{p}: full architectural state')

# A second implementation using exact rational arithmetic, independent of
# SoftFloat and the simulator, rounds finite arithmetic under all five modes.
def value(b):
    e=(b>>23)&255
    sig=(b&0x7fffff)+(0x800000 if e else 0)
    shift=(e-127-23) if e else -149
    q=Fraction(sig<<shift) if shift>=0 else Fraction(sig,1<<-shift)
    return -q if b>>31 else q

def rounded(q,rm):
    sign=int(q<0)
    if not q:return (0x80000000 if rm==2 else 0),0
    q=abs(q);n,d=q.numerator,q.denominator
    e=n.bit_length()-d.bit_length()
    if (n < d<<e) if e>=0 else (n<<-e < d):e-=1
    step=max(e-23,-149)
    if step>=0:d<<=step
    else:n<<=-step
    sig,rem=divmod(n,d)
    inc=bool(rem) and ((rm==0 and (2*rem>d or (2*rem==d and sig&1))) or
                     (rm==2 and sign) or (rm==3 and not sign) or (rm==4 and 2*rem>=d))
    sig+=inc
    if sig>=1<<24:sig>>=1;e+=1
    if e>127:
        inf=rm in (0,4) or (rm==2 and sign) or (rm==3 and not sign)
        return (sign<<31)|(0x7f800000 if inf else 0x7f7fffff),5
    if e < -126:
        bits=sig # subnormal, or rounding up to minimum normal
    else:bits=((e+127)<<23)|(sig&0x7fffff)
    flags=int(bool(rem))
    if rem and bits<0x800000:flags|=2
    return (sign<<31)|bits,flags

rng=random.Random(10032)
requests=[];answers=[]
for op in (0,1,2,3,5):
    for _ in range(250):
        a,b,c=[rng.randrange(1,0x7f800000)|(rng.randrange(2)<<31) for _ in range(3)]
        A,B,C=map(value,(a,b,c))
        q={0:lambda:A+B,1:lambda:A-B,2:lambda:A*B,3:lambda:A/B,5:lambda:A*B+C}[op]()
        for rm in range(5):
            requests.append(f'{op} {rm} {a:x} {b:x} {c:x}')
            answers.append(rounded(q,rm))

# Positive square roots: locate adjacent representable values using integer
# binary search and compare their exact squared midpoint (no host sqrt).
for _ in range(125):
    a=rng.randrange(1,0x7f800000);x=value(a)
    lo,hi=0,0x7f7fffff
    while lo<hi:
        mid=(lo+hi+1)//2
        if value(mid)**2<=x:lo=mid
        else:hi=mid-1
    exact=value(lo)**2==x
    midpoint=(value(lo)+value(lo+1))/2
    for rm in range(5):
        up=not exact and (rm==3 or (rm in (0,4) and
             (x>midpoint**2 or (x==midpoint**2 and (rm==4 or lo&1)))))
        requests.append(f'4 {rm} {a:x} 0 0');answers.append((lo+int(up),int(not exact)))

def rounded_integer(q,rm):
    sign=int(q<0);q=abs(q);n,d=q.numerator,q.denominator
    i,rem=divmod(n,d)
    up=bool(rem) and ((rm==0 and (2*rem>d or (2*rem==d and i&1))) or
        (rm==2 and sign) or (rm==3 and not sign) or (rm==4 and 2*rem>=d))
    return -(i+up) if sign else i+up,bool(rem)

for _ in range(100):
    a=rng.randrange(1,0x7f800000)|(rng.randrange(2)<<31)
    for op in (6,7):
        for rm in range(5):
            i,inexact=rounded_integer(value(a),rm)
            low,high=(0,0xffffffff) if op==7 else (-0x80000000,0x7fffffff)
            invalid=i<low or i>high
            clipped=min(max(i,low),high)&0xffffffff
            requests.append(f'{op} {rm} {a:x} 0 0')
            answers.append((clipped,16 if invalid else int(inexact)))
    a=rng.randrange(1,0x100000000)
    for op in (8,9):
        i=a if op==9 or a<0x80000000 else a-0x100000000
        for rm in range(5):
            requests.append(f'{op} {rm} {a:x} 0 0');answers.append(rounded(Fraction(i),rm))

proc=subprocess.run([str(ROOT/'tests/fp_checks'),'--vectors'],input='\n'.join(requests)+'\n',text=True,capture_output=True,check=True,timeout=20)
lines=proc.stdout.splitlines();need(len(lines)==len(answers),'vector output length')
for request,line,expected in zip(requests,lines,answers):
    got=tuple(int(x,16) for x in line.split())
    need(got==expected,f'exact rational mismatch {request}: {got} != {expected}')

# Scratch execution plus delayed fcsr commit must preserve precise exceptions.
for p in sorted((ROOT/'tests/fp_regression').glob('*.s')):
    subprocess.run([str(ROOT/'tests/pipeline_checks'),str(p)],check=True,stdout=subprocess.DEVNULL,timeout=20)
    need(True,f'{p}: precise complete state, all safe modes')

expected_registers={
 'unsigned_conversion':{'s0':0xffffff00,'s1':0xffffffff,'s2':0xbf800000,'s3':1,'s4':17,'s5':3},
 'third_operand':{'s0':6,'s1':7},
 'csr_rounding':{'s0':0,'s1':1,'s2':1,'s3':9,'s4':3,'s5':8,'s6':0x88,'s7':0xfffffffd,'s8':2,'s9':31,'s10':0x40},
 'integer_m':{'s0':0x80000000,'s1':0,'s2':0x80000000,'s3':0x7fffffff,
              's4':0x80000000,'s5':0,'s6':0xffffffff,'s7':0x80000000,'s8':0xffffffff,
              's9':0xffffffff,'s10':1,'s11':0x7fffffff}
}
for name,expected in expected_registers.items():
    out=run(ROOT/'tests/fp_regression'/f'{name}.s','--single-cycle')
    got={n:int(v,16) for n,v in re.findall(r'x\d+\s*/(\w+)\s*=0x([0-9a-f]+)',out)}
    for reg,v in expected.items():need(got[reg]==v,f'{name}: expected {reg}={v:#x}')
    if name=='csr_rounding':need(int(fcsr(out),16)==0x40,'CSR final fcsr')
# Assembler must reject swapped register files and unsupported rounding names.
import tempfile
bad_instructions=['fadd.s x1, f2, f3','add f1, x2, x3','flw f1, 0(f2)',
                  'fmadd.s f1, f2, f3','fcvt.w.s x1, f2, bogus',
                  'csrrwi x1, frm, 32','.float 1.0junk']
for instruction in bad_instructions:
    with tempfile.TemporaryDirectory() as temp:
        tmp=Path(temp)/'bad.s';tmp.write_text('main:\n    '+instruction+'\n')
        r=subprocess.run([str(ROOT/'rvsim'),str(tmp),'--single-cycle'],capture_output=True,text=True)
        need(r.returncode!=0 and 'assembly error' in r.stderr,f'assembler rejects {instruction}')
print(f'PASS: {checks} W10 checks, including {len(answers)} independent exact-rational rounding vectors')
