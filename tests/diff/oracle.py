#!/usr/bin/env python3
"""Dev-only full-state single-instruction oracle; no product linkage."""
import argparse
import random
import subprocess
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_MODE_64
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_MODE_64

p = argparse.ArgumentParser()
p.add_argument('runner')
p.add_argument('--backend', default='interp', choices=['interp', 'jit'])
args = p.parse_args()
rng = random.Random(0x455845434F4E)
regs64 = [UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RBX,
          UC_X86_REG_RSP, UC_X86_REG_RBP, UC_X86_REG_RSI, UC_X86_REG_RDI,
          UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_R10, UC_X86_REG_R11,
          UC_X86_REG_R12, UC_X86_REG_R13, UC_X86_REG_R14, UC_X86_REG_R15]
regs32 = [UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EBX,
          UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI]
edges = [0, 1, 0xf, 0x10, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff,
         0x7fffffff, 0x80000000, 0xffffffff, 0x7fffffffffffffff,
         0x8000000000000000, 0xffffffffffffffff]
cases = []
for bits in (64, 32):
    if args.backend == 'jit' and bits == 32:
        continue
    for width in (8, 16, 32, 64):
        if width == 64 and bits == 32:
            continue
        prefix = b'\x66' if width == 16 else b'\x48' if width == 64 else b''
        for opcode in (0x00, 0x08, 0x20, 0x28, 0x30, 0x38, 0x84, 0x88):
            op = opcode + (width != 8)
            # register-register, [rbp+disp8], SIB and reverse-direction forms
            operands = [b'\xd8', b'\x5d\x08', b'\x5c\x75\x10']
            if args.backend == 'jit':
                operands = [b'\xd8']
            for operand in operands:
                code = prefix + bytes([op]) + operand
                for n in range(20):
                    values = [rng.getrandbits(bits) for _ in range(16)]
                    values[0] = edges[n % len(edges)] & ((1 << bits)-1)
                    values[3] = rng.choice(edges) & ((1 << bits)-1)
                    values[4] = 0x200800
                    values[5] = 0x200100
                    values[6] = 8
                    flags = 2 | (rng.getrandbits(12) & 0xcd5)
                    defined = 0xcd5
                    if opcode in (0x08, 0x20, 0x30, 0x84):
                        defined &= ~0x10  # AF undefined for logical operations.
                    cases.append((bits, code, values, flags, defined))
        if args.backend == 'interp':
            for tail in (b'\xff\xc0', b'\xff\xc8', b'\xf7\xd8', b'\xd1\xe0', b'\xd1\xe8', b'\xd1\xf8', b'\xd3\xe0', b'\xd3\xf8'):
                if width == 8:
                    tail = bytes([tail[0]-1]) + tail[1:]
                for n in range(24):
                    values = [rng.getrandbits(bits) for _ in range(16)]
                    values[0] = rng.choice(edges) & ((1 << bits)-1)
                    count = rng.choice([0, 1, 2, 7, 8, 15, 16, 31, 32, 63, 64])
                    values[1] = count
                    flags = 2 | (rng.getrandbits(12) & 0xcd5)
                    defined = 0xcd5
                    if tail[0] in (0xd0, 0xd1, 0xd2, 0xd3):
                        effective = (count if tail[0] in (0xd2, 0xd3) else 1) & (63 if width == 64 else 31)
                        if effective:
                            defined &= ~0x10
                        if effective > 1:
                            defined &= ~0x800
                        if effective >= width:
                            defined &= ~1
                    cases.append((bits, prefix+tail, values, flags, defined))
    if args.backend == 'interp':
        for code in [b'\x50', b'\x58', b'\x54', b'\x5c', b'\x6a\xff', b'\x0f\xb6\xc4', b'\x0f\xbe\xc4', b'\x0f\x44\xc3', b'\x0f\x94\xc0', b'\x75\x10', b'\x8d\x44\x75\x08']:
            for n in range(20):
                values = [rng.getrandbits(bits) for _ in range(16)]
                values[4] = 0x200800
                values[5] = 0x200100
                values[6] = 8
                cases.append((bits, code, values, 2 | (rng.getrandbits(12)&0xcd5), 0xcd5))
requests = []
for bits, code, values, flags, _ in cases:
    requests.append(f'{args.backend} {bits} {code.hex()} ' + ' '.join(f'{v:x}' for v in values) + f' {flags:x}')
result = subprocess.run([args.runner], input='\n'.join(requests)+'\n', text=True, capture_output=True, check=True)
lines = result.stdout.splitlines()
assert len(lines) == len(cases), (len(lines), len(cases), result.stderr)
for index, ((bits, code, values, flags, defined), line) in enumerate(zip(cases, lines)):
    assert not line.startswith('error'), (index, code.hex(), line)
    fields = line.split()
    actual = [int(v, 16) for v in fields[:19]]
    u = Uc(UC_ARCH_X86, UC_MODE_64 if bits == 64 else UC_MODE_32)
    u.mem_map(0x100000, 4096)
    u.mem_map(0x200000, 4096)
    u.mem_write(0x100000, code)
    u.mem_write(0x200000, bytes((i*13+7)&255 for i in range(4096)))
    registers = regs64 if bits == 64 else regs32
    for reg, value in zip(registers, values):
        u.reg_write(reg, value)
    u.reg_write(UC_X86_REG_EFLAGS, flags)
    u.emu_start(0x100000, 0x100000+len(code), count=1)
    expected = [u.reg_read(reg) for reg in registers]
    if bits == 32:
        expected += values[8:]
    expected += [u.reg_read(UC_X86_REG_EFLAGS), u.reg_read(UC_X86_REG_RIP if bits == 64 else UC_X86_REG_EIP), len(code)]
    actual[16] &= defined
    expected[16] &= defined
    assert actual == expected, (index, bits, code.hex(), values, hex(flags), actual, expected)
    assert bytes.fromhex(fields[19]) == bytes(u.mem_read(0x200000, 4096)), (index, code.hex(), 'memory mismatch')
print(f'PASS {len(cases)} full-register/defined-flags/4096-byte-memory {args.backend} vs Unicorn cases')
if args.backend == 'interp':
    # All successful custom decodes must agree with independent Capstone lengths.
    decodes = [(bits, rng.randbytes(15)) for bits in (32, 64) for _ in range(10000)]
    payload = '\n'.join(f'decode {bits} {code.hex()}' for bits, code in decodes)+'\n'
    output = subprocess.run([args.runner], input=payload, capture_output=True, text=True, check=True).stdout.splitlines()
    assert len(output) == len(decodes)
    engines = {32: Cs(CS_ARCH_X86, CS_MODE_32), 64: Cs(CS_ARCH_X86, CS_MODE_64)}
    accepted = 0
    for (bits, code), line in zip(decodes, output):
        if line.startswith('error'):
            continue
        decoded = list(engines[bits].disasm(code, 0x100000, count=1))
        assert decoded and decoded[0].size == int(line), (bits, code.hex(), line, decoded)
        accepted += 1
    print(f'PASS 20,000 random decoder inputs; {accepted} accepted lengths match Capstone')
