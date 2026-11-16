#!/usr/bin/env python3
"""W6-W9 regressions; floating-point fixtures are intentionally excluded."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
CHECKS = 0

def require(ok, message):
    global CHECKS
    CHECKS += 1
    if not ok:
        raise AssertionError(message)

def run(path, *args):
    r = subprocess.run([str(ROOT / 'rvsim'), str(path), *args], cwd=ROOT,
                       text=True, capture_output=True, timeout=10, check=True)
    require(not r.stderr or (args == ('--hazard', 'none') and
            all(line.startswith('[warning] unhandled ecall') for line in r.stderr.splitlines())),
            f'{path}: unexpected stderr: {r.stderr}')
    return r.stdout

def regs(s):
    return re.findall(r'x\d+\s*/\w+\s*=0x[0-9a-f]+', s)

def output(s):
    return s.split('\n', 1)[1].split('---------------------------- registers')[0]

def status(s):
    return re.search(r'status = (\w+)\s+cycles = (\d+)', s).groups()

def stats(s):
    return {k: float(v) if '.' in v else int(v)
            for k, v in re.findall(r'(\w+)=([0-9.]+)', s.split('pipeline:')[1])}

files = sorted((ROOT/'tests').glob('*.s'))
files += sorted((ROOT/'tests/given').glob('lab[123]_*/*.s'))
for path in files:
    oracle = run(path, '--single-cycle')
    require(status(oracle)[0] == 'ECALL_EXIT', f'{path}: oracle did not exit')
    for mode in ('--pipeline', '--pipeline-forward', '--hazard stall', '--hazard forward'):
        got = run(path, *mode.split())
        require(regs(got) == regs(oracle) and output(got) == output(oracle), f'{path} {mode}: values/output')
        require(status(got)[0] == 'ECALL_EXIT', f'{path} {mode}: status')
        st = stats(got)
        require(st['retired'] == int(status(oracle)[1]), f'{path} {mode}: retired')
        penalty = 2 if mode.startswith('--hazard') else 1
        require(st['cycles'] == st['retired'] + st['stalls'] + penalty*st['flushes'] + 4 - st['discarded_stalls'],
                f'{path} {mode}: cycle accounting')
    subprocess.run([str(ROOT/'tests/pipeline_checks'), str(path)], cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL, timeout=10)
    require(True, f'{path}: full architectural state')

# Supplied goldens: compare values, output, status, timing, and all supplied
# counters. PC is the implementation-specific fetch pointer, so excluded.
for lab, modes in [('lab2_pipeline', {'single-cycle': '--single-cycle', 'pipeline': '--pipeline-basic'}),
                   ('lab3_hazards', {'single-cycle': '--single-cycle', **{p: '--hazard '+p for p in ('stall','forward','none')}})]:
    for path in sorted((ROOT/'tests/given'/lab).glob('*.s')):
        for label, mode in modes.items():
            expected = (path.parent/'expected'/f'{path.stem}.{label}.out').read_text()
            got = run(path, *mode.split())
            require(regs(got) == regs(expected), f'{path} {label}: golden registers')
            require(output(got) == output(expected), f'{path} {label}: golden output')
            require(status(got) == status(expected), f'{path} {label}: golden status/cycles')
            if label != 'single-cycle':
                st = stats(got)
                mapping = {'retired': st['retired'], 'CPI': st['CPI'], 'stalls': st['stalls'],
                           'forwards': st['fwd_ex']+st['fwd_mem'], 'redirects': st['flushes'],
                           'squashed': 2*st['flushes']}
                for key, value in mapping.items():
                    match = re.search(r'^'+key+r'\s*=\s*([0-9.]+)', expected, re.M)
                    if match:
                        require(value == float(match[1]), f'{path} {label}: golden {key}: {value} != {match[1]}')

for path in sorted((ROOT/'tests/regression').glob('*.s')):
    subprocess.run([str(ROOT/'tests/pipeline_checks'), str(path)], cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL, timeout=10)
    require(True, f'{path}: precise state')

for name, mode, wanted in [
    ('sample1_sum_loop', '--pipeline', (108,53,40,11,0,0,0)),
    ('sample1_sum_loop', '--pipeline-forward', (68,53,0,11,13,11,0)),
    ('sample2_mem_and_branches', '--pipeline', (26,13,8,1,0,0,0)),
    ('sample2_mem_and_branches', '--pipeline-forward', (18,13,0,1,2,2,0)),
    ('micro', '--pipeline', (15,5,6,0,0,0,0)),
    ('micro', '--pipeline-forward', (9,5,0,0,2,1,0))]:
    st = stats(run(ROOT/'tests'/f'{name}.s', mode))
    require(tuple(st[k] for k in ('cycles','retired','stalls','flushes','fwd_ex','fwd_mem','load_use')) == wanted,
            f'{name} {mode}: sheet golden counters')
for mode in ('--pipeline', '--pipeline-forward', '--hazard stall', '--hazard forward'):
    limited = run(ROOT/'tests/micro.s', *mode.split(), '--max-cycles', '3')
    require(status(limited) == ('RUNNING', '3') and 'reached max cycle limit' in limited,
            f'{mode}: cycle limit')
    trace = subprocess.run([str(ROOT/'rvsim'), 'tests/micro.s', *mode.split(), '--trace'],
                           cwd=ROOT, capture_output=True, text=True, check=True)
    require('[cycle 1]' in trace.stderr and 'WB' in trace.stderr, f'{mode}: trace')

# W9 supplied goldens and independent oracle comparisons for both safe hazards.
branch_files = sorted((ROOT/'tests/given/lab4_branch').glob('*.s'))
for path in branch_files:
    oracle = run(path, '--single-cycle')
    expected = (path.parent/'expected'/f'{path.stem}.single-cycle.out').read_text()
    require(regs(oracle) == regs(expected) and output(oracle) == output(expected)
            and status(oracle) == status(expected), f'{path}: W9 oracle golden')
    for pred in ('not-taken', 'btfn', '1bit', '2bit'):
        for hazard in ('forward', 'stall'):
            got = run(path, '--predictor', pred, '--hazard', hazard)
            require(regs(got) == regs(oracle) and output(got) == output(oracle)
                    and status(got)[0] == 'ECALL_EXIT', f'{path} {pred} {hazard}: oracle')
            st = stats(got)
            require(st['retired'] == int(status(oracle)[1]), f'{path}: W9 retired')
            require(st['redirects'] == st['mispredicts'] + st['jalr'], f'{path}: redirects')
            require(st['cycles'] == st['retired'] + 4 + st['stalls'] + 2*st['redirects'] - st['discarded_stalls'],
                    f'{path}: W9 cycle accounting')
            if hazard == 'forward':
                expected = (path.parent/'expected'/f'{path.stem}.{pred}.out').read_text()
                require(status(got) == status(expected), f'{path} {pred}: golden cycles')
                mapping = {**st, 'forwards': st['fwd_ex']+st['fwd_mem']}
                for key in ('retired','CPI','stalls','forwards','branches','mispredicts',
                            'accuracy','jal','jalr','redirects','squashed'):
                    match = re.search(r'^'+key+r'\s*=\s*([0-9.]+)', expected, re.M)
                    require(match is not None and mapping[key] == float(match[1]),
                            f'{path} {pred}: golden {key}')
    subprocess.run([str(ROOT/'tests/pipeline_checks'), str(path)], cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL, timeout=20)
    require(True, f'{path}: W9 full memory state, all predictors and safe hazards')

for pred in ('not-taken','btfn','1bit','2bit'):
    got = run(ROOT/'tests/regression/wrong_path_branch.s', '--predictor', pred)
    st = stats(got)
    require(st['branches'] == 1 and st['jal'] == 0, f'{pred}: wrong-path training/count')
    got = run(ROOT/'tests/regression/predicted_loop_exit.s', '--predictor', pred)
    require('x8 /s0  =0x00000002' in got, f'{pred}: predicted-taken exit state')
    limited = run(ROOT/'tests/micro.s', '--predictor', pred, '--max-cycles', '3')
    require(status(limited) == ('RUNNING','3'), f'{pred}: W9 cycle limit')
# Predictor/hazard order must not change semantics or timing.
f = branch_files[0]
a = run(f, '--predictor','2bit','--hazard','stall')
b = run(f, '--hazard','stall','--predictor','2bit')
require(a == b, 'predictor/hazard option order')
invalid = subprocess.run([str(ROOT/'rvsim'),str(f),'--predictor','bad'], capture_output=True, text=True)
require(invalid.returncode != 0 and 'invalid predictor' in invalid.stderr, 'unknown predictor')
print(f'PASS: {CHECKS} checks; W6-W9 goldens, counters, full architectural state, faults, limits, traces')
