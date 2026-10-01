#!/usr/bin/env python3
"""解析 EAF 表情资源的帧/块结构，对比不同表情的编码特征。

EAF 文件格式（参照 gfx_eaf_dec.h/c）:
  文件头:
  [0]    magic 0x89
  [1:4]  "EAF"
  [4:8]  total_frames u32
  [8:12] checksum u32
  [12:16] table_len u32
  [16..] 帧表 total_frames × {frame_size u32, frame_offset u32}
  帧数据区 = 16 + total_frames*8, 每帧由 frame_offset 定位, 以 0x5A5A 魔数开头
  帧内 (跳过 2 字节魔数后):
  [0:2]  "_S"
  [3:9]  version (6 bytes)
  [9]    bit_depth
  [10]   width  u16
  [12]   height u16
  [14]   blocks u16
  [16]   block_height u16
  [18..] block_len u32 * blocks -> 块数据, 每块首字节为 encoding type
"""
import struct
import sys
from collections import Counter

ENC = {0: "RLE", 1: "HUFFMAN", 2: "JPEG", 3: "HUFFMAN_DIRECT", 4: "HEATSHRINK", 5: "RAW"}


def parse_frame(data, off, frame_size):
    if len(data) - off < 20:
        return None
    magic = struct.unpack_from("<H", data, off)[0]
    if magic != 0x5A5A:
        return {"note": f"bad frame magic 0x{magic:04x}"}
    base = off + 2
    fmt = data[base:base+2]
    if fmt != b"_S":
        return {"format": fmt.decode(errors="replace"), "note": "non-_S frame"}
    bit_depth = data[base+9]
    width, height = struct.unpack_from("<HH", data, base+10)
    blocks, block_height = struct.unpack_from("<HH", data, base+14)
    if width == 0 or height == 0 or blocks == 0 or blocks > 512:
        return None
    blens = struct.unpack_from("<%dI" % blocks, data, base+18)
    num_colors = 0 if bit_depth == 24 else (1 << bit_depth)
    cursor = base + 18 + blocks * 4 + num_colors * 4  # 跳过调色板 (gfx_eaf_dec.c data_offset)
    encs = Counter()
    for i, blen in enumerate(blens):
        if blen == 0 or cursor + blen > len(data):
            encs["OVERFLOW"] += 1
            break
        enc = data[cursor]
        encs[ENC.get(enc, "UNKNOWN_%d" % enc)] += 1
        cursor += blen
    return {
        "format": fmt.decode(),
        "version": data[base+3:base+9].decode(errors="replace"),
        "bit_depth": bit_depth,
        "width": width, "height": height,
        "blocks": blocks, "block_height": block_height,
        "encodings": encs,
        "data_bytes": cursor - base,
        "frame_size_hdr": frame_size,
    }


def parse_file(path):
    data = open(path, "rb").read()
    assert data[0] == 0x89 and data[1:4] == b"EAF", f"bad magic: {path}"
    total = struct.unpack_from("<I", data, 4)[0]
    table_off = 16
    entries = []
    for i in range(total):
        size, off = struct.unpack_from("<II", data, table_off + i * 8)
        entries.append((size, off))
    data_base = table_off + total * 8
    parsed = []
    for size, off in entries:
        f = parse_frame(data, data_base + off, size)
        if f is None:
            parsed.append((size, off, {"note": "parse failed"}))
            continue
        parsed.append((size, off, f))
    return data, entries, parsed


def main(paths):
    for p in paths:
        try:
            data, frames, parsed = parse_file(p)
        except Exception as e:
            print(f"{p}: PARSE ERROR {e}")
            continue
        n = len(parsed)
        print(f"=== {p.split('/')[-1]}: {len(data)} bytes, {n} frames ===")
        if not parsed:
            print("  (no frames parsed)")
            continue
        # 汇总特征
        versions = Counter(f[2].get("version", "?") for f in parsed)
        bds = Counter(f[2].get("bit_depth", "?") for f in parsed)
        dims = Counter((f[2].get("width"), f[2].get("height")) for f in parsed)
        blocks = Counter(f[2].get("blocks", "?") for f in parsed)
        bh = Counter(f[2].get("block_height", "?") for f in parsed)
        encs = Counter()
        for _, _, f in parsed:
            if "encodings" in f:
                encs.update(f["encodings"])
        sizes = [s for s, _, _ in parsed]
        print(f"  version: {dict(versions)}")
        print(f"  bit_depth: {dict(bds)}")
        print(f"  dims: {dict(dims)}")
        print(f"  blocks: {dict(blocks)}, block_height: {dict(bh)}")
        print(f"  encodings(块数): {dict(encs)}")
        print(f"  frame data size: min={min(sizes)} max={max(sizes)} avg={sum(sizes)//n}")
        # 逐帧
        for i, (s, _, f) in enumerate(parsed):
            e = dict(f.get("encodings", {}))
            if "note" in f:
                print(f"    [{i:02d}] {s:>7} B  {f['note']}")
                continue
            print(f"    [{i:02d}] {s:>7} B  v={f['version']} bd={f['bit_depth']} "
                  f"{f['width']}x{f['height']} blocks={f['blocks']} bh={f['block_height']} enc={e}")
        print()


if __name__ == "__main__":
    paths = sys.argv[1:]
    main(paths)
