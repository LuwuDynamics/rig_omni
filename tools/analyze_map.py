#!/usr/bin/env python3
"""分析 ESP32-S3 固件 map 文件的内存分布。

用法: python3 analyze_map.py build/rig-puppy.map
"""
import re
import sys
from collections import defaultdict


def parse_map(path):
    lines = open(path, encoding="utf-8", errors="replace").readlines()
    syms = defaultdict(list)  # section -> [(size, name, addr)]
    for i, line in enumerate(lines):
        m = re.match(r"^\s+(\.\S+)\s*$", line)
        if not m:
            continue
        sec = m.group(1)
        if i + 1 >= len(lines):
            continue
        m2 = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)", lines[i + 1])
        if m2:
            addr = int(m2.group(1), 16)
            size = int(m2.group(2), 16)
            name = m2.group(3)
            syms[sec].append((size, name, addr))
    return syms


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "build/rig-puppy.map"
    syms = parse_map(path)

    def collect(prefixes):
        """按段名前缀聚合符号."""
        items = []
        for sec, lst in syms.items():
            if any(sec.startswith(p) for p in prefixes):
                items.extend(lst)
        return sorted(items, reverse=True)

    def dump(title, prefixes):
        items = collect(prefixes)
        tot = sum(i[0] for i in items)
        print(f"=== {title}: {len(items)} 个符号, 合计 {tot} bytes ({tot/1024:.1f} KB) ===")
        for size, name, addr in items[:12]:
            print(f"    {size:>8}  0x{addr:08x}  {name}")
        print()

    dump("DRAM .bss（零初始化静态数据）", [".bss", ".dram0.bss", ".dram1.bss"])
    dump("DRAM .data（有初值静态数据）", [".data", ".dram0.data", ".dram1.data"])
    dump("DRAM .noinit", [".noinit"])
    dump("PSRAM 静态段 (.ext_ram.*)", [".ext_ram"])

    # 找出最大的静态分配（全部段）
    print("=== 全部静态符号 Top 20（按大小） ===")
    all_syms = [(s[0], s[1], sec) for sec, lst in syms.items() for s in lst]
    all_syms.sort(reverse=True)
    for size, name, sec in all_syms[:20]:
        print(f"  {size:>8}  [{sec}]  {name}")


if __name__ == "__main__":
    main()
