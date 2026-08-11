#!/usr/bin/env python3
"""coreinfo.py — extract crash registers + mapping table from a busierbox ARM core.

Reads the NT_PRSTATUS/NT_FILE notes from an ELF core produced on the t4 device
(docs/99-debugging methodology). With the PC and the library base from NT_FILE,
blame the fault on a library + virtual offset.

Usage: coreinfo.py <core> [extra lib base:offset=path ...]
Prints registers, signal, fault address (NT_SIGINFO), and the NT_FILE map.
"""
import struct
import sys

def elf_notes(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF':
        sys.exit('not an ELF')
    eclass = d[4]
    if eclass != 1:
        sys.exit('only 32-bit ELF supported')
    isle = d[5] == 1
    E = '<' if isle else '>'
    # 32-bit ELF header
    e_phoff = struct.unpack_from(E + 'I', d, 28)[0]
    e_phentsize = struct.unpack_from(E + 'H', d, 42)[0]
    e_phnum = struct.unpack_from(E + 'H', d, 44)[0]
    notes = []
    for i in range(e_phnum):
        ph = e_phoff + i * e_phentsize
        p_type = struct.unpack_from(E + 'I', d, ph)[0]
        if p_type == 4:  # PT_NOTE
            p_offset = struct.unpack_from(E + 'I', d, ph + 4)[0]
            p_filesz = struct.unpack_from(E + 'I', d, ph + 16)[0]
            notes.append((p_offset, p_offset + p_filesz))
    return d, E, notes

def parse_notes(d, E, ranges):
    out = []
    for (start, end) in ranges:
        o = start
        while o + 12 <= end:
            namesz, descsz, ntype = struct.unpack_from(E + 'III', d, o)
            name = d[o + 12: o + 12 + namesz].rstrip(b'\0')
            dstart = (o + 12 + namesz + 3) & ~3
            desc = d[dstart: dstart + descsz]
            out.append((name, ntype, desc))
            o = (dstart + descsz + 3) & ~3
    return out

def main():
    d, E, notes = elf_notes(sys.argv[1])
    found = {}
    for (name, ntype, desc) in parse_notes(d, E, notes):
        if ntype == 1 and name == b'CORE':          # NT_PRSTATUS
            # ARM 32-bit prstatus: regs at +70 (0x46), 17 words
            regs = struct.unpack_from(E + '17I', desc, 0x46)
            names = ['r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7',
                     'r8', 'r9', 'r10', 'r11', 'r12', 'r13_sp',
                     'r14_lr', 'r15_pc', 'cpsr']
            found['regs'] = dict(zip(names, regs))
            found['sig'] = struct.unpack_from(E + 'i', desc, 0)[0]
            found['pid'] = struct.unpack_from(E + 'i', desc, 28)[0]
        elif ntype == 4 and name == b'CORE':        # NT_SIGINFO
            code = struct.unpack_from(E + 'iii', desc, 0)
            found['siginfo'] = code
        elif ntype == 9 and name == b'CORE':        # NT_FILE
            pagesz = struct.unpack_from(E + 'I', desc, 0)[0]
            nf = struct.unpack_from(E + 'I', desc, 4)[0]
            np = struct.unpack_from(E + 'I', desc, 8)[0]
            hdr = 12
            starts = struct.unpack_from(E + '%di' % nf, desc, hdr)
            hdr += 4 * nf
            ends = struct.unpack_from(E + '%di' % nf, desc, hdr)
            hdr += 4 * nf
            foffs = struct.unpack_from(E + '%di' % nf, desc, hdr)
            hdr += 4 * np
            fnames = [desc[hdr:].split(b'\0')[0].decode('utf-8', 'replace')]
            # multiple files are NUL-packed; split all
            names = []
            rest = desc[hdr:]
            for _ in range(np):
                i = rest.index(b'\0')
                names.append(rest[:i].decode('utf-8', 'replace'))
                rest = rest[i + 1:]
            found['maps'] = list(zip(starts, ends, foffs, names))
    regs = found.get('regs', {})
    if not regs:
        sys.exit('no NT_PRSTATUS found')
    print('pid=%d signal=%d' % (found.get('pid'), found.get('sig')))
    if 'siginfo' in found:
        si = found['siginfo']
        print('siginfo: signo=%d errno=%d code=%d (fault addr in r-args)' % si)
    for n in ['r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7',
              'r8', 'r9', 'r10', 'r11', 'r12', 'r13_sp', 'r14_lr',
              'r15_pc', 'cpsr']:
        print('  %-8s 0x%08x' % (n, regs[n]))
    print('mapped files:')
    for (s, e, fo, name) in found.get('maps', []):
        print('  %08x-%08x off=%x %s' % (s, e, fo, name))

if __name__ == '__main__':
    main()