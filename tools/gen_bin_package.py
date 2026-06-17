#!/usr/bin/env python3
"""
构建后自动生成 bin 目录，包含可上传到服务器的固件文件
"""

import os
import shutil
import json
import argparse
import re

# 项目根目录
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")

# 分区偏移地址
OFFSETS = {
    "bootloader": 0,
    "partition-table": 32768,      # 0x8000
    "app": 131072,                 # 0x20000
    "assets": 8388608,             # 0x800000
}

def get_version():
    """从 CMakeLists.txt 读取 PROJECT_VER 版本号"""
    cmake_file = os.path.join(PROJECT_ROOT, "CMakeLists.txt")
    if os.path.exists(cmake_file):
        try:
            with open(cmake_file, 'r') as f:
                content = f.read()
                # 匹配 set(PROJECT_VER "x.x.x")
                match = re.search(r'set\s*\(\s*PROJECT_VER\s+"([^"]+)"\s*\)', content)
                if match:
                    return match.group(1)
        except:
            pass
    
    # 备用：从 build/project_description.json 读取
    desc_file = os.path.join(BUILD_DIR, "project_description.json")
    if os.path.exists(desc_file):
        try:
            with open(desc_file, 'r') as f:
                data = json.load(f)
                return data.get("version", "0.0.0")
        except:
            pass
    
    return "0.0.0"

def get_board_type():
    """从 sdkconfig 读取板型，返回 (bin_name, display_name)"""
    sdkconfig = os.path.join(PROJECT_ROOT, "sdkconfig")
    if os.path.exists(sdkconfig):
        try:
            with open(sdkconfig, 'r') as f:
                content = f.read()
                if "CONFIG_BOARD_TYPE_PUPPY=y" in content:
                    return "rig-puppy", "RIG Puppy"
                elif "CONFIG_BOARD_TYPE_HOVER=y" in content:
                    return "rig-hover", "RIG Hover"
        except:
            pass
    return "rig-puppy", "RIG Puppy"

def copy_bin_files():
    """复制 bin 文件到输出目录"""
    bin_name, display_name = get_board_type()
    files_to_copy = [
        ("build/bootloader/bootloader.bin", "bootloader.bin"),
        ("build/partition_table/partition-table.bin", "partition-table.bin"),
        (f"build/{bin_name}.bin", f"{bin_name}.bin"),
        ("tools/spiffs_assets/build/assets.bin", "assets.bin"),
    ]
    
    for src_rel, dst_name in files_to_copy:
        src_path = os.path.join(PROJECT_ROOT, src_rel)
        dst_path = os.path.join(BIN_DIR, dst_name)
        
        if os.path.exists(src_path):
            shutil.copy2(src_path, dst_path)
            size_kb = os.path.getsize(dst_path) / 1024
            print(f"  ✓ {dst_name} ({size_kb:.1f} KB)")
        else:
            print(f"  ✗ {dst_name} - 源文件不存在: {src_rel}")

def generate_manifest(version):
    """生成完整烧录的 manifest.json"""
    bin_name, display_name = get_board_type()
    manifest = {
        "name": f"{display_name}固件",
        "version": version,
        "home_assistant_domain": "",
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "improv": False,
                "parts": [
                    {"path": "bootloader.bin", "offset": OFFSETS["bootloader"]},
                    {"path": "partition-table.bin", "offset": OFFSETS["partition-table"]},
                    {"path": f"{bin_name}.bin", "offset": OFFSETS["app"]},
                    {"path": "assets.bin", "offset": OFFSETS["assets"]},
                ]
            }
        ]
    }
    
    manifest_path = os.path.join(BIN_DIR, "manifest.json")
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(f"  ✓ manifest.json (完整烧录)")

def generate_manifest_app(version):
    """生成仅应用固件的 manifest_app.json"""
    bin_name, display_name = get_board_type()
    manifest = {
        "name": f"{display_name}应用固件",
        "version": version,
        "home_assistant_domain": "",
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "improv": False,
                "parts": [
                    {"path": f"{bin_name}.bin", "offset": OFFSETS["app"]},
                ]
            }
        ]
    }
    
    manifest_path = os.path.join(BIN_DIR, "manifest_app.json")
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(f"  ✓ manifest_app.json (应用固件)")

def generate_manifest_assets(version):
    """生成仅资源文件的 manifest_assets.json"""
    bin_name, display_name = get_board_type()
    manifest = {
        "name": f"{display_name}资源文件",
        "version": version,
        "home_assistant_domain": "",
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "improv": False,
                "parts": [
                    {"path": "assets.bin", "offset": OFFSETS["assets"]},
                ]
            }
        ]
    }
    
    manifest_path = os.path.join(BIN_DIR, "manifest_assets.json")
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(f"  ✓ manifest_assets.json (资源文件)")

def main():
    parser = argparse.ArgumentParser(description='生成固件发布包')
    parser.add_argument('--version', '-v', help='固件版本号')
    args = parser.parse_args()
    
    version = args.version or get_version()
    
    print(f"\n📦 生成固件发布包 v{version}")
    print("=" * 40)
    
    # 创建输出目录
    if os.path.exists(BIN_DIR):
        shutil.rmtree(BIN_DIR)
    os.makedirs(BIN_DIR)
    print(f"输出目录: {BIN_DIR}\n")
    
    # 复制 bin 文件
    print("📄 复制固件文件:")
    copy_bin_files()
    
    # 生成 manifest 文件
    print("\n📋 生成 manifest 文件:")
    generate_manifest(version)
    generate_manifest_app(version)
    generate_manifest_assets(version)
    
    # 统计
    total_size = sum(
        os.path.getsize(os.path.join(BIN_DIR, f)) 
        for f in os.listdir(BIN_DIR) 
        if f.endswith('.bin')
    )
    print(f"\n✅ 完成! 总大小: {total_size/1024/1024:.2f} MB")
    print(f"📁 文件位置: {BIN_DIR}")

if __name__ == "__main__":
    main()
