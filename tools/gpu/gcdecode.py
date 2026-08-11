#!/usr/bin/env python3
"""Decode a gcvHAL_COMMIT command-buffer dump (t4ioctl shim output).

Reads the hex words from a trace file, decodes the GC1000 command stream.
Draw command = 16 bytes {0x28000000, format, count, first} per
gcoHARDWARE_DrawPrimitives (libGAL+0xb2f60). LOAD_STATE format from
gcoHARDWARE_LoadState32 (+0x9d3d8).
"""
import re, sys

def parse_words(path):
    words = []
    for line in open(path):
        m = re.match(r'\s*[0-9a-f]+:', line)
        if not m: continue
        for tok in line[m.end():].split():
            if re.fullmatch(r'[0-9a-f]{8}', tok):
                words.append(int(tok, 16))
    return words

def decode(words):
    i = 0
    n = len(words)
    out = []
    while i < n:
        w = words[i]
        top = w & 0xff000000
        if (w >> 28) == 0x2:
            # draw command: 0x28000000 family, 4 words
            fmt = words[i+1] if i+1 < n else -1
            cnt = words[i+2] if i+2 < n else -1
            first = words[i+3] if i+3 < n else -1
            out.append(f"@0x{i*4:05x} DRAW cmd=0x{w:08x} fmt={fmt:#x} count={cnt} first={first}")
            i += 4
        elif top == 0x08000000:
            # LOAD_STATE: word = 0x08000000 | count<<16 | addr? decode count/addr
            cnt = (w >> 16) & 0xfff
            addr = w & 0xffff
            data = words[i+1] if i+1 < n else -1
            out.append(f"@0x{i*4:05x} LOAD_STATE addr=0x{addr:04x} cnt={cnt} data=0x{data:08x}")
            i += 2
        elif (w >> 16) == 0x1000:
            op = w & 0xff
            out.append(f"@0x{i*4:05x} CMD_0x1000{op:02x} = 0x{w:08x}")
            i += 1
        elif w == 0:
            out.append(f"@0x{i*4:05x} zero")
            i += 1
        else:
            out.append(f"@0x{i*4:05x} ? 0x{w:08x}")
            i += 1
    return out

if __name__ == "__main__":
    for path in sys.argv[1:]:
        print(f"=== {path} ===")
        for line in decode(parse_words(path)):
            print(line)
