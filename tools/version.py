#!/usr/bin/env python3
"""板级版本号管理工具

用法:
    python tools/version.py list              # 列出所有板子的版本号
    python tools/version.py set <board> <ver> # 设置指定板子的版本号
    python tools/version.py get <board>       # 查看指定板子的版本号

示例:
    python tools/version.py list
    python tools/version.py set arm 3.7.1
    python tools/version.py get puppy
"""

import os
import sys
import re

# 项目根目录（脚本位于 tools/ 下）
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARDS_DIR = os.path.join(PROJECT_ROOT, "main", "boards")

# 支持的板子列表（从 boards 目录自动发现含有 version.txt 的子目录）
SUPPORTED_BOARDS = []


def discover_boards():
    """自动发现所有包含 version.txt 的板子目录"""
    boards = []
    if os.path.isdir(BOARDS_DIR):
        for name in sorted(os.listdir(BOARDS_DIR)):
            board_path = os.path.join(BOARDS_DIR, name)
            if os.path.isdir(board_path) and os.path.exists(os.path.join(board_path, "version.txt")):
                boards.append(name)
    return boards


def get_version_path(board: str) -> str:
    return os.path.join(BOARDS_DIR, board, "version.txt")


def read_version(board: str) -> str:
    """读取指定板子的版本号"""
    path = get_version_path(board)
    if not os.path.exists(path):
        return None
    with open(path, "r") as f:
        return f.read().strip()


def write_version(board: str, version: str):
    """写入指定板子的版本号"""
    path = get_version_path(board)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(version.strip() + "\n")


def validate_version(version: str) -> bool:
    """验证版本号格式（仅允许 x.y.z 或 x.y）"""
    return bool(re.match(r"^\d+\.\d+(\.\d+)?$", version))


def cmd_list():
    """列出所有板子的版本号"""
    boards = discover_boards()
    if not boards:
        print("未找到任何板子版本文件，请先在 main/boards/<board>/version.txt 中创建。")
        return

    max_name_len = max(len(b) for b in boards)
    print(f"{'Board':<{max_name_len}}  Version")
    print("-" * (max_name_len + 10))
    for b in boards:
        ver = read_version(b)
        print(f"{b:<{max_name_len}}  {ver}")


def cmd_get(board: str):
    """查看指定板子的版本号"""
    ver = read_version(board)
    if ver is None:
        print(f"[错误] 板子 '{board}' 不存在或没有 version.txt")
        sys.exit(1)
    print(ver)


def cmd_set(board: str, version: str):
    """设置指定板子的版本号"""
    if not validate_version(version):
        print(f"[错误] 版本号格式无效: '{version}'，请使用 x.y.z 或 x.y 格式")
        sys.exit(1)

    if not os.path.exists(get_version_path(board)):
        print(f"[错误] 板子 '{board}' 不存在或没有 version.txt")
        print(f"  支持的板子: {', '.join(discover_boards())}")
        sys.exit(1)

    old_ver = read_version(board)
    write_version(board, version)
    print(f"[OK] {board}: {old_ver} -> {version}")


def print_usage():
    print(__doc__)


def main():
    args = sys.argv[1:]

    if not args:
        print_usage()
        sys.exit(0)

    cmd = args[0].lower()

    if cmd in ("list", "ls", "show"):
        cmd_list()
    elif cmd == "get":
        if len(args) < 2:
            print("[错误] 缺少板子名称。用法: python tools/version.py get <board>")
            sys.exit(1)
        cmd_get(args[1])
    elif cmd == "set":
        if len(args) < 3:
            print("[错误] 缺少参数。用法: python tools/version.py set <board> <version>")
            sys.exit(1)
        cmd_set(args[1], args[2])
    elif cmd in ("-h", "--help", "help"):
        print_usage()
    else:
        print(f"[错误] 未知命令: {cmd}")
        print_usage()
        sys.exit(1)


if __name__ == "__main__":
    main()
