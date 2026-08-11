#!/usr/bin/env python3
"""ptsamp.py <pid> [samples] [interval] — minimal PTRACE_GETREGS sampler.
arm32: PTRACE_GETREGS=12, pt_regs = 18 longs, pc = uregs[15], sp = [13],
cpsr = [16], orig_r0 = [17]. r0 = [0], lr = [14]."""
import ctypes, os, sys, time

PTRACE_ATTACH = 16
PTRACE_DETACH = 17
PTRACE_GETREGS = 12
PTRACE_PEEKDATA = 3

libc = ctypes.CDLL("libc.so.6", use_errno=True)
ptrace = libc.ptrace
ptrace.argtypes = [ctypes.c_long, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
ptrace.restype = ctypes.c_long

pid = int(sys.argv[1])
n = int(sys.argv[2]) if len(sys.argv) > 2 else 5
iv = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0

regs = (ctypes.c_long * 18)()

for i in range(n):
    r = ptrace(PTRACE_ATTACH, pid, None, None)
    if r != 0:
        err = ctypes.get_errno()
        print(f"attach fail errno={err}", flush=True)
        sys.exit(1)
    os.waitpid(pid, 0)
    r = ptrace(PTRACE_GETREGS, pid, None, ctypes.byref(regs))
    if r != 0:
        print("getregs fail", ctypes.get_errno(), flush=True)
    else:
        print(f"[{i}] pc=0x{regs[15]:08x} lr=0x{regs[14]:08x} sp=0x{regs[13]:08x} "
              f"r0=0x{regs[0]:08x} r1=0x{regs[1]:08x} r2=0x{regs[2]:08x} "
              f"r3=0x{regs[3]:08x} r4=0x{regs[4]:08x} r5=0x{regs[5]:08x} "
              f"r6=0x{regs[6]:08x} r7=0x{regs[7]:08x} "
              f"cpsr=0x{regs[16]:08x}", flush=True)
    sp0 = regs[13] & 0xffffffff
    try:
        mem = open(f"/proc/{pid}/mem", "rb")
        mem.seek(sp0 - 0x40)
        data = mem.read(0x100)
        mem.close()
        import struct
        for off in range(0, 0x100, 4):
            w = struct.unpack_from("<I", data, off)[0]
            if w > 0xb6000000 and w < 0xb8000000:
                print(f"    [sp-0x40+{off:02x}]=0x{w:08x}", flush=True)
    except Exception as e:
        print(f"    mem read fail: {e}", flush=True)
    ptrace(PTRACE_DETACH, pid, None, None)
    if i < n - 1:
        time.sleep(iv)
