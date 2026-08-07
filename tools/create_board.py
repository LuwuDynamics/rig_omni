#!/usr/bin/env python3
"""
RIG-Omni 板子创建向导

用法:
    python tools/create_board.py
    
交互式创建新的开发板，自动生成所有必要文件并在构建系统中注册。
板子专属音效只需把 .ogg 文件放在板子目录下，编译时自动嵌入。
"""

import os
import sys
import shutil
import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BOARDS_DIR = PROJECT_ROOT / "main" / "boards"


def ask(prompt, default=None, validate=None):
    """交互式提问"""
    while True:
        if default:
            val = input(f"{prompt} [{default}]: ").strip()
            if not val:
                val = default
        else:
            val = input(f"{prompt}: ").strip()
        if validate and not validate(val):
            print(f"  ❌ 输入无效，请重试")
            continue
        return val


def ask_choice(prompt, options):
    """选择题"""
    print(f"\n{prompt}")
    for i, opt in enumerate(options, 1):
        label = opt if isinstance(opt, str) else opt[0]
        print(f"  {i}. {label}")
    while True:
        try:
            choice = int(input(f"请选择 [1-{len(options)}]: ").strip())
            if 1 <= choice <= len(options):
                return options[choice - 1]
        except ValueError:
            pass
        print("  ❌ 输入无效")


def confirm(msg):
    """确认"""
    return input(f"\n{msg} (y/n) [y]: ").strip().lower() in ("", "y", "yes")


def slugify(name):
    """board short name → valid C/CMake identifier"""
    return re.sub(r'[^a-z0-9_]', '_', name.lower().replace('-', '_'))


# ─── 步骤 1: 收集用户输入 ──────────────────────────────────

def collect_board_info():
    print("=" * 60)
    print("  RIG-Omni 板子创建向导")
    print("=" * 60)

    info = {}

    # 1. 板子名称
    info["board_dir"] = ask(
        "\n📁 板子目录名（只能小写字母+数字+下划线）",
        validate=lambda v: bool(re.match(r'^[a-z][a-z0-9_]*$', v))
    )
    default_display = "RIG-" + info["board_dir"].replace('_', '-').title()
    info["board_display"] = ask("🏷️  板子显示名 (Kconfig 菜单中显示)", default=default_display)

    # 2. 描述
    info["description"] = ask("📝 板子简介（一行）", default=f"{info['board_display']} 开发板")

    # 3. 项目名
    default_bin = "rig-" + info["board_dir"].replace('_', '-')
    info["project_bin"] = ask("🔧 输出二进制名", default=default_bin)

    # 4. Kconfig 标识符
    default_kconfig = "BOARD_TYPE_" + info["board_dir"].upper()
    info["kconfig_id"] = ask("⚙️  Kconfig 标识符", default=default_kconfig)

    # 5. 选择模板
    available = [d.name for d in BOARDS_DIR.iterdir()
                 if d.is_dir() and d.name not in ("common",)]
    choice = ask_choice("📋 选择参考模板（你最接近的现有板子）", available)
    info["base_board"] = choice if isinstance(choice, str) else choice[0]

    # 6. 舵机数量
    info["motor_num"] = int(ask("🔩 舵机数量 (0 = 无舵机)", default="0"))

    # 7. UART 配置（有舵机时）
    # 后续板子统一使用 Hover/ARM 的引脚配置（与 Puppy 不同）
    if info["motor_num"] > 0:
        info["uart_tx"] = "GPIO_NUM_46"
        info["uart_rx"] = "GPIO_NUM_38"
        info["uart_baud"] = "500000"
        info["laser_gpio"] = "GPIO_NUM_3"
        info["touch_gpio"] = "GPIO_NUM_3"
        info["xgo_interval"] = ask("  xgo_task 循环间隔 (ms)", default="2")
        info["xgo_rx_interval"] = ask("  xgo_rx_task 循环间隔 (ms)", default="20")
        # BLE 遥控
        info["has_ble"] = confirm("  支持 BLE 遥控?")
    else:
        info["has_ble"] = False

    # 8. IMU
    if confirm("\n🧭 是否需要 IMU?"):
        info["has_imu"] = True
        info["imu_sda"] = ask("  IMU I2C SDA 引脚", default="GPIO_NUM_14")
        info["imu_scl"] = ask("  IMU I2C SCL 引脚", default="GPIO_NUM_48")
    else:
        info["has_imu"] = False

    # 9. 显示（分辨率固定 240x240）
    if confirm("\n🖥️  是否需要 LCD 显示?"):
        info["has_display"] = True
        info["display_resolution"] = "240_240"
        print("  显示方向配置:")
        info["display_mirror_x"] = confirm("    Mirror X?")
        info["display_mirror_y"] = confirm("    Mirror Y?")
        info["display_swap_xy"] = confirm("    Swap XY?")
        info["display_invert"] = confirm("    Invert Color?")
    else:
        info["has_display"] = False

    # 10. 摄像头
    info["has_camera"] = confirm("\n📷 是否需要摄像头?")
    if info["has_camera"]:
        rotation = ask_choice("摄像头旋转角度", ["0", "90", "180", "270"])
        info["camera_rotation"] = rotation if isinstance(rotation, str) else rotation[0]

    # 11. 唤醒词
    if confirm("\n🗣️  是否需要唤醒词?"):
        info["has_wakenet"] = True
        info["wakenet_model"] = ask("  唤醒词模型", default="wn9_xiaolutongxue")
    else:
        info["has_wakenet"] = False

    # 汇总
    print("\n" + "=" * 60)
    print("  📋 配置汇总")
    print("=" * 60)
    for k, v in info.items():
        print(f"  {k}: {v}")

    if not confirm("确认创建?"):
        print("已取消")
        sys.exit(0)

    return info


# ─── 步骤 2: 创建板子目录和文件 ────────────────────────────

def create_board_files(info):
    board_path = BOARDS_DIR / info["board_dir"]
    base_path = BOARDS_DIR / info["base_board"]

    if board_path.exists():
        print(f"\n❌ 目录已存在: {board_path}")
        sys.exit(1)

    print(f"\n📁 创建板子目录: {board_path}")
    os.makedirs(board_path, exist_ok=True)

    # ── config.json ──
    config_json = {
        "target": "esp32s3",
        "display": info.get("has_display", False),
        "camera": info.get("has_camera", False),
        "psram": True,
        "description": info["description"]
    }
    import json
    (board_path / "config.json").write_text(
        json.dumps(config_json, indent=2, ensure_ascii=False) + "\n"
    )

    # ── board_config.h ──
    guard = f"_{info['board_dir'].upper()}_BOARD_CONFIG_H_"
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        f"// ===================================================================",
        f"// {info['board_display']} 板型专属配置",
        f"// ===================================================================",
    ]

    if info["motor_num"] > 0:
        lines += [
            "",
            "// XGO / Motor UART Config",
            f"#define XGO_UART_TX_PIN {info['uart_tx']}",
            f"#define XGO_UART_RX_PIN {info['uart_rx']}",
            "",
            "// Laser Control Pin",
            f"#define LASER_GPIO {info['laser_gpio']}",
            "",
            f"#define TOUCH_BUTTON_GPIO {info['touch_gpio']}",
        ]

    if info.get("has_imu"):
        lines += [
            "",
            "// IMU I2C Config",
            f"#define IMU_I2C_SDA {info['imu_sda']}",
            f"#define IMU_I2C_SCL {info['imu_scl']}",
        ]

    if info["motor_num"] > 0:
        lines += [
            "",
            "// XGO Task Config",
            f"#define XGO_TASK_INTERVAL_MS     {info['xgo_interval']}   "
            "// xgo_task 循环间隔（毫秒）",
            f"#define XGO_RX_TASK_INTERVAL_MS  {info['xgo_rx_interval']}  "
            "// xgo_rx_task 循环间隔（毫秒）",
        ]

    if info.get("has_display"):
        lines += [
            "",
            "// Display 方向配置",
            f"#define DISPLAY_MIRROR_X {'true' if info.get('display_mirror_x') else 'false'}",
            f"#define DISPLAY_MIRROR_Y {'true' if info.get('display_mirror_y') else 'false'}",
            f"#define DISPLAY_SWAP_XY {'true' if info.get('display_swap_xy') else 'false'}",
            f"#define DISPLAY_INVERT_COLOR {'true' if info.get('display_invert') else 'false'}",
        ]

    if info.get("has_camera"):
        lines += [
            "",
            "// Camera 方向配置",
            f"#define CAMERA_ROTATION {info.get('camera_rotation', '0')}",
        ]

    lines += [
        "",
        f"#endif // {guard}",
        "",
    ]
    (board_path / "board_config.h").write_text("\n".join(lines))

    # ── 复制 xgo 相关文件（有舵机时）──
    if info["motor_num"] > 0:
        for fname in ["xgo.h", "xgo.cc", "xgo_action.h", "xgo_action.cc"]:
            src = base_path / fname
            if src.exists():
                dst = board_path / fname
                content = src.read_text()
                # MOTOR_NUM 替换
                old_motor = re.search(r'#define MOTOR_NUM\s+(\d+)', content)
                if old_motor:
                    content = content.replace(
                        old_motor.group(0),
                        f"#define MOTOR_NUM {info['motor_num']}"
                    )
                dst.write_text(content)
                print(f"  ✓ 复制 {fname}")

        # 如果有 ik 文件也复制
        for fname in ["ik.h", "ik.cc"]:
            src = base_path / fname
            if src.exists():
                shutil.copy2(src, board_path / fname)
                print(f"  ✓ 复制 {fname}")

        # BLE 遥控文件
        if info.get("has_ble"):
            for fname in ["ble_remote_control.h", "ble_remote_control.cc"]:
                src = base_path / fname
                if src.exists():
                    shutil.copy2(src, board_path / fname)
                    print(f"  ✓ 复制 {fname}")

    # ── 板子 .cc 文件（从模板创建）──
    board_cc_name = f"{info['board_dir']}_board.cc"
    src_board_cc = base_path / f"{info['base_board']}_board.cc"
    if src_board_cc.exists():
        content = src_board_cc.read_text()
        # 替换类名等
        old_class = info['base_board'].title() + "Board"
        new_class = info['board_dir'].title() + "Board"
        content = content.replace(old_class, new_class)
        content = content.replace(f'"{info["base_board"].upper()}"', f'"{info["board_dir"].upper()}"')

        (board_path / board_cc_name).write_text(content)
        print(f"  ✓ 创建 {board_cc_name} (从 {info['base_board']}_board.cc)")

    # ── 链接表情目录 ──
    emoji_src = base_path / "emoji"
    emoji_dst = board_path / "emoji"
    if emoji_src.exists() and not emoji_dst.exists():
        # 使用相对路径创建软链接
        rel_path = os.path.relpath(emoji_src, board_path)
        os.symlink(rel_path, emoji_dst)
        print(f"  ✓ 链接 emoji/ → {rel_path}")

    # ── 链接唤醒词 ──
    if info.get("has_wakenet"):
        wakenet_src = base_path / "wakenet"
        wakenet_dst = board_path / "wakenet"
        if wakenet_src.exists() and not wakenet_dst.exists():
            rel_path = os.path.relpath(wakenet_src, board_path)
            os.symlink(rel_path, wakenet_dst)
            print(f"  ✓ 链接 wakenet/ → {rel_path}")

    # ── 链接分辨率目录 ──
    if info.get("has_display"):
        res_dir = info["display_resolution"]
        res_src = base_path / res_dir
        res_dst = board_path / res_dir
        if res_src.exists() and not res_dst.exists():
            rel_path = os.path.relpath(res_src, board_path)
            os.symlink(rel_path, res_dst)
            print(f"  ✓ 链接 {res_dir}/ → {rel_path}")

    # ── 创建示例音效文件（占位）──
    placeholder = board_path / "PUT_YOUR_OGG_FILES_HERE.txt"
    placeholder.write_text(
        "将板子专属音效 .ogg 文件放在此目录即可\n"
        "编译时自动发现并嵌入固件，无需手动配置\n"
        "\n"
        "例如:\n"
        "  - your_startup_sound.ogg\n"
        "  - your_notification.ogg\n"
    )
    print(f"  ✓ 音效目录就绪（放 .ogg 即自动嵌入）")

    print(f"\n✅ 板子文件已创建: {board_path}")
    return board_path


# ─── 步骤 3: 注册到构建系统 ────────────────────────────────

def register_in_build_system(info):
    print("\n🔧 注册到构建系统...")

    # ── Kconfig.projbuild ──
    # 在 BOARD_TYPE choice 的 endchoice 之前插入新板子
    kconfig_path = PROJECT_ROOT / "main" / "Kconfig.projbuild"
    kconfig_lines = kconfig_path.read_text().splitlines(keepends=True)

    # 找到 choice BOARD_TYPE 后的第一个 endchoice
    in_choice = False
    endchoice_idx = None
    for i, line in enumerate(kconfig_lines):
        stripped = line.strip()
        if stripped.startswith("choice BOARD_TYPE"):
            in_choice = True
        elif in_choice and stripped == "endchoice":
            endchoice_idx = i
            break

    if endchoice_idx:
        new_entry = [
            f"    config {info['kconfig_id']}\n",
            f'        bool "{info["board_display"]}"\n',
            "        depends on IDF_TARGET_ESP32S3\n",
        ]
        for line in reversed(new_entry):
            kconfig_lines.insert(endchoice_idx, line)
        kconfig_path.write_text("".join(kconfig_lines))
        print(f"  ✓ Kconfig.projbuild 已注册 {info['kconfig_id']}")
    else:
        print("  ⚠️  未找到 Kconfig BOARD_TYPE endchoice，请手动添加")

    # 同时把 PUPPY_ENABLE_BLE_CONTROL 依赖也加上（如果有 BLE）
    if info.get("has_ble"):
        kconfig_lines = kconfig_path.read_text().splitlines(keepends=True)
        for i, line in enumerate(kconfig_lines):
            if "depends on BOARD_TYPE_PUPPY || BOARD_TYPE_HOVER || BOARD_TYPE_ARM" in line:
                old = line
                new = line.rstrip() + f" || {info['kconfig_id']}" + "\n"
                kconfig_lines[i] = new
                break
        kconfig_path.write_text("".join(kconfig_lines))
        print(f"  ✓ Kconfig BLE 依赖已扩展")

    # 同时扩展 EMOTE 等其他 depends 依赖
    kconfig_lines = kconfig_path.read_text().splitlines(keepends=True)
    for i, line in enumerate(kconfig_lines):
        if "depends on BOARD_TYPE_PUPPY || BOARD_TYPE_HOVER || BOARD_TYPE_ARM" in line:
            if info['kconfig_id'] not in line:
                kconfig_lines[i] = line.rstrip() + f" || {info['kconfig_id']}" + "\n"
    kconfig_path.write_text("".join(kconfig_lines))
    print(f"  ✓ Kconfig depends 已扩展")

    # ── 根 CMakeLists.txt ──
    # 在最后一个 elseif+set(PROJECT_BIN_NAME) 之后、endif() 之前插入
    root_cmake = PROJECT_ROOT / "CMakeLists.txt"
    root_lines = root_cmake.read_text().splitlines(keepends=True)

    # 找到 "set(PROJECT_BIN_NAME "rig-arm")" 这行（最后一个）
    last_set_idx = None
    for i, line in enumerate(root_lines):
        if 'set(PROJECT_BIN_NAME "rig-' in line:
            last_set_idx = i

    if last_set_idx:
        # 紧接着的 endif() 在下一行
        endif_idx = None
        for i in range(last_set_idx + 1, min(last_set_idx + 3, len(root_lines))):
            if "endif()" in root_lines[i]:
                endif_idx = i
                break
        if endif_idx:
            new_entry = [
                f'        elseif(LINE MATCHES "CONFIG_{info["kconfig_id"]}=y")\n',
                f'            set(PROJECT_BIN_NAME "{info["project_bin"]}")\n',
            ]
            for line in reversed(new_entry):
                root_lines.insert(endif_idx, line)
            root_cmake.write_text("".join(root_lines))
            print(f"  ✓ 根 CMakeLists.txt 已注册 {info['project_bin']}")
        else:
            print("  ⚠️  未找到根 CMakeLists endif()，请手动添加")
    else:
        print("  ⚠️  未找到根 CMakeLists 板子注册，请手动添加")

    # ── main/CMakeLists.txt ──
    # 在最后一个 servo board (ARM) 块的 endif() 之前插入
    main_cmake = PROJECT_ROOT / "main" / "CMakeLists.txt"
    main_lines = main_cmake.read_text().splitlines(keepends=True)

    # 找到最后一个 CONFIG_BOARD_TYPE_ARM 块对应的 endif()
    # 结构: elseif(CONFIG_BOARD_TYPE_ARM) ... endif()
    arm_idx = None
    for i, line in enumerate(main_lines):
        if "CONFIG_BOARD_TYPE_ARM)" in line:
            arm_idx = i

    if arm_idx:
        # 找到这个 elseif 块之后的第一个 endif()（不在嵌套中）
        # 由于 board elseif 块内没有嵌套，下一个 endif() 就是
        endif_idx = None
        for i in range(arm_idx + 1, len(main_lines)):
            if main_lines[i].strip() == "endif()":
                endif_idx = i
                break

        if endif_idx:
            new_block = [
                f"elseif(CONFIG_{info['kconfig_id']})\n",
                f'    set(BOARD_DIR "{info["board_dir"]}")\n',
                f'    set(BOARD_TYPE "{info["board_display"]}")\n',
                f'    set(PROJECT_BIN_NAME "{info["project_bin"]}")\n',
            ]

            if info.get("has_display"):
                new_block += [
                    "    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)\n",
                    "    set(BUILTIN_ICON_FONT font_puhui_basic_20_4)\n",
                    "    # 使用自定义 EAF 表情\n",
                    '    set(EMOTE_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}")\n',
                    f'    set(EMOTE_RESOLUTION "{info["display_resolution"]}")\n',
                ]
            if info.get("has_wakenet"):
                new_block += [
                    "    # 唤醒词模型配置\n",
                    f'    set(WAKENET_MODEL "{info["wakenet_model"]}")\n',
                    '    set(WAKENET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/wakenet/${WAKENET_MODEL}")\n',
                ]
            if info.get("has_display") or info.get("has_wakenet"):
                new_block.append("    set(BUILD_CUSTOM_ASSETS TRUE)\n")

            # 始终加自动音效发现
            new_block += [
                "    # 自动收集板子专属音效（.ogg 文件放在 board/ 目录即可）\n",
                '    file(GLOB BOARD_OGG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/*.ogg")\n',
                "    list(APPEND BOARD_EMBED_FILES ${BOARD_OGG_FILES})\n",
            ]

            for line in reversed(new_block):
                main_lines.insert(endif_idx, line)
            main_cmake.write_text("".join(main_lines))
            print(f"  ✓ main/CMakeLists.txt 已注册 BOARD_DIR={info['board_dir']}")
        else:
            print("  ⚠️  未找到 main/CMakeLists endif()，请手动添加")
    else:
        print("  ⚠️  未找到 ARM 板子注册点，请手动添加")

    print("\n" + "=" * 60)
    print("  ✅ 板子创建完成！")
    print("=" * 60)
    print(f"""
  下一步:
    1. 编辑 {BOARDS_DIR / info['board_dir'] / f'{info["board_dir"]}_board.cc'}
       完善你的板子逻辑（类名: {info['board_dir'].title()}Board）

    2. 把专属音效 .ogg 文件放到板子目录下（自动嵌入）

    3. 运行 idf.py menuconfig 选择 {info['board_display']} 编译
""")


# ─── 主流程 ─────────────────────────────────────────────────

def main():
    info = collect_board_info()
    create_board_files(info)
    register_in_build_system(info)


if __name__ == "__main__":
    main()
