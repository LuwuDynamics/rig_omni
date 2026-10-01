#!/usr/bin/env python3
"""
RIG-Omni 板子创建向导

用法:
    python tools/create_board.py

交互式创建新开发板：生成板级文件，并在 Kconfig / CMake 中注册。
电机协议在 main/boards/common/motors/（按 IC：em3 / scs009 / kp4012），
本向导只复制板级组态（motor.cc）和机器人逻辑，不复制寄存器数字。
"""

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BOARDS_DIR = PROJECT_ROOT / "main" / "boards"

# 每种电机 IC 从哪块现有板拷 motor.h/.cc（组态模板，协议仍走 common/motors）
MOTOR_IC_SOURCE = {
    "em3": "puppy",
    "scs009": "arm",
    "em3_kp4012": "hover",
}

MOTOR_IC_CHOICES = [
    ("em3", "EM3 舵机（Puppy 同类）"),
    ("scs009", "SCS009 舵机（Arm / Tars 同类）"),
    ("em3_kp4012", "EM3 舵机 + KP4012 轮毂（Hover 同类）"),
    ("none", "暂不选，稍后自己写 motor.cc"),
]


def ask(prompt, default=None, validate=None):
    while True:
        if default:
            val = input(f"{prompt} [{default}]: ").strip()
            if not val:
                val = default
        else:
            val = input(f"{prompt}: ").strip()
        if validate and not validate(val):
            print("  ❌ 输入无效，请重试")
            continue
        return val


def ask_choice(prompt, options):
    print(f"\n{prompt}")
    for i, opt in enumerate(options, 1):
        label = opt if isinstance(opt, str) else opt[1]
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
    return input(f"\n{msg} (y/n) [y]: ").strip().lower() in ("", "y", "yes")


def link_dir(src, dst, label):
    src = Path(src)
    dst = Path(dst)
    if not src.exists() or dst.exists() or dst.is_symlink():
        return

    rel_path = os.path.relpath(src, dst.parent)
    try:
        os.symlink(rel_path, dst, target_is_directory=True)
        print(f"  ✓ 链接 {label}/ → {rel_path}")
        return
    except OSError:
        if sys.platform != "win32":
            raise

    result = subprocess.run(
        ["cmd", "/c", "mklink", "/J", str(dst), str(src.resolve())],
        capture_output=True,
        text=True,
    )
    if result.returncode == 0:
        print(f"  ✓ 连接 {label}/ → {src} (junction)")
        return

    shutil.copytree(src, dst)
    print(f"  ✓ 复制 {label}/ （无法创建链接，已复制）")


def replace_motor_num(content, motor_num):
    return re.sub(
        r"#define MOTOR_NUM\s+\d+",
        f"#define MOTOR_NUM {motor_num}",
        content,
        count=1,
    )


def copy_text(src, dst, transform=None):
    src = Path(src)
    if not src.exists():
        return False
    content = src.read_text(encoding="utf-8")
    if transform:
        content = transform(content)
    dst.write_text(content, encoding="utf-8")
    print(f"  ✓ 复制 {src.name} → {dst.name}")
    return True


def collect_board_info():
    print("=" * 60)
    print("  RIG-Omni 板子创建向导")
    print("=" * 60)

    info = {}

    info["board_dir"] = ask(
        "\n📁 板子目录名（只能小写字母+数字+下划线）",
        validate=lambda v: bool(re.match(r"^[a-z][a-z0-9_]*$", v)),
    )
    default_display = "RIG-" + info["board_dir"].replace("_", "-").title()
    info["board_display"] = ask("🏷️  板子显示名 (Kconfig 菜单中显示)", default=default_display)
    info["description"] = ask("📝 板子简介（一行）", default=f"{info['board_display']} 开发板")
    default_bin = "rig-" + info["board_dir"].replace("_", "-")
    info["project_bin"] = ask("🔧 输出二进制名", default=default_bin)
    default_kconfig = "BOARD_TYPE_" + info["board_dir"].upper()
    info["kconfig_id"] = ask("⚙️  Kconfig 标识符", default=default_kconfig)

    available = sorted(
        d.name for d in BOARDS_DIR.iterdir()
        if d.is_dir() and d.name not in ("common",)
    )
    if not available:
        print("❌ 找不到可用模板板")
        sys.exit(1)
    choice = ask_choice("📋 选择参考模板（整体结构最接近的现有板子）", available)
    info["base_board"] = choice if isinstance(choice, str) else choice[0]

    info["motor_num"] = int(ask("🔩 舵机数量 (0 = 无舵机)", default="0"))
    info["motor_ic"] = "none"
    if info["motor_num"] > 0:
        ic = ask_choice(
            "⚡ 电机 IC（协议在 boards/common/motors/，此处只选组态模板）",
            MOTOR_IC_CHOICES,
        )
        info["motor_ic"] = ic[0] if isinstance(ic, tuple) else ic
        info["uart_tx"] = "GPIO_NUM_46"
        info["uart_rx"] = "GPIO_NUM_38"
        info["laser_gpio"] = "GPIO_NUM_3"
        info["touch_gpio"] = "GPIO_NUM_3"
        info["robot_interval"] = ask("  robot_task 循环间隔 (ms)", default="4")
        info["motor_rx_interval"] = ask("  motor_rx_task 循环间隔 (ms)", default="20")
        info["has_ble"] = confirm("  支持 BLE 遥控?")
        print("  UART TX=GPIO46  RX=GPIO38（与 Arm/Hover 默认一致；Puppy 是 TX=GPIO3）")
    else:
        info["has_ble"] = False

    if confirm("\n🧭 是否需要 IMU?"):
        info["has_imu"] = True
        info["imu_sda"] = ask("  IMU I2C SDA 引脚", default="GPIO_NUM_14")
        info["imu_scl"] = ask("  IMU I2C SCL 引脚", default="GPIO_NUM_48")
    else:
        info["has_imu"] = False

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

    info["has_camera"] = confirm("\n📷 是否需要摄像头?")
    if info["has_camera"]:
        rotation = ask_choice("摄像头旋转角度", ["0", "90", "180", "270"])
        info["camera_rotation"] = rotation if isinstance(rotation, str) else rotation[0]

    if confirm("\n🗣️  是否需要唤醒词?"):
        info["has_wakenet"] = True
        info["wakenet_model"] = ask("  唤醒词模型", default="wn9_xiaolutongxue")
    else:
        info["has_wakenet"] = False

    print("\n" + "=" * 60)
    print("  📋 配置汇总")
    print("=" * 60)
    for k, v in info.items():
        print(f"  {k}: {v}")

    if not confirm("确认创建?"):
        print("已取消")
        sys.exit(0)

    return info


def create_board_files(info):
    board_path = BOARDS_DIR / info["board_dir"]
    base_path = BOARDS_DIR / info["base_board"]

    if board_path.exists():
        print(f"\n❌ 目录已存在: {board_path}")
        sys.exit(1)

    print(f"\n📁 创建板子目录: {board_path}")
    os.makedirs(board_path, exist_ok=True)

    config_json = {
        "target": "esp32s3",
        "display": info.get("has_display", False),
        "camera": info.get("has_camera", False),
        "psram": True,
        "description": info["description"],
    }
    (board_path / "config.json").write_text(
        json.dumps(config_json, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    guard = f"_{info['board_dir'].upper()}_BOARD_CONFIG_H_"
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "// ===================================================================",
        f"// {info['board_display']} 板型专属配置",
        "// ===================================================================",
    ]

    if info["motor_num"] > 0:
        lines += [
            "",
            "// Motor UART Config",
            f"#define MOTOR_UART_TX_PIN {info['uart_tx']}",
            f"#define MOTOR_UART_RX_PIN {info['uart_rx']}",
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
            "// Robot / Motor Task Config",
            f"#define ROBOT_TASK_INTERVAL_MS     {info['robot_interval']}    "
            "// robot_task 循环间隔（毫秒），控制舵机指令发送频率",
            f"#define MOTOR_RX_TASK_INTERVAL_MS  {info['motor_rx_interval']}   "
            "// motor_rx_task 循环间隔（毫秒），控制舵机反馈和 IMU 读取频率",
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

    lines += ["", f"#endif // {guard}", ""]
    (board_path / "board_config.h").write_text("\n".join(lines), encoding="utf-8")
    print("  ✓ 创建 board_config.h")

    if info["motor_num"] > 0:
        ic = info.get("motor_ic", "none")
        motor_src_dir = MOTOR_IC_SOURCE.get(ic)
        motor_src = BOARDS_DIR / motor_src_dir if motor_src_dir else base_path

        def motor_transform(content):
            return replace_motor_num(content, info["motor_num"])

        for fname in ("motor.h", "motor.cc"):
            src = motor_src / fname
            if not copy_text(src, board_path / fname, motor_transform):
                print(f"  ⚠️  未找到 {src}，请自行添加板级 motor 组态")

        for fname in ("robot.h", "robot.cc", "robot_action.h", "robot_action.cc"):
            copy_text(base_path / fname, board_path / fname, motor_transform)

        for fname in ("ik.h", "ik.cc"):
            src = base_path / fname
            if src.exists():
                shutil.copy2(src, board_path / fname)
                print(f"  ✓ 复制 {fname}")

        if info.get("has_ble"):
            for fname in ("ble_remote_control.h", "ble_remote_control.cc"):
                src = base_path / fname
                if src.exists():
                    shutil.copy2(src, board_path / fname)
                    print(f"  ✓ 复制 {fname}")

        print("  ℹ 电机协议请用 boards/common/motors/<型号>.h，不要把寄存器数字抄进板级文件")

    board_cc_name = f"{info['board_dir']}_board.cc"
    src_board_cc = base_path / f"{info['base_board']}_board.cc"
    if src_board_cc.exists():
        content = src_board_cc.read_text(encoding="utf-8")
        old_class = info["base_board"].title() + "Board"
        new_class = info["board_dir"].title() + "Board"
        content = content.replace(old_class, new_class)
        content = content.replace(f'"{info["base_board"].upper()}"', f'"{info["board_dir"].upper()}"')
        content = content.replace("XGO_UART_TX_PIN", "MOTOR_UART_TX_PIN")
        content = content.replace("XGO_UART_RX_PIN", "MOTOR_UART_RX_PIN")
        content = content.replace("XGO_TASK_INTERVAL_MS", "ROBOT_TASK_INTERVAL_MS")
        content = content.replace("XGO_RX_TASK_INTERVAL_MS", "MOTOR_RX_TASK_INTERVAL_MS")
        content = content.replace("xgo_task", "robot_task")
        content = content.replace("xgo_rx", "motor_rx")
        content = content.replace("xgo_control", "robot_control")
        (board_path / board_cc_name).write_text(content, encoding="utf-8")
        print(f"  ✓ 创建 {board_cc_name} (从 {info['base_board']}_board.cc)")

    ver_src = base_path / "version.txt"
    if ver_src.exists():
        shutil.copy2(ver_src, board_path / "version.txt")
        print("  ✓ 复制 version.txt（请改成新板版本号）")
    else:
        (board_path / "version.txt").write_text("0.1.0\n", encoding="utf-8")
        print("  ✓ 创建 version.txt")

    link_dir(base_path / "emoji", board_path / "emoji", "emoji")
    if info.get("has_wakenet"):
        link_dir(base_path / "wakenet", board_path / "wakenet", "wakenet")
    if info.get("has_display"):
        res_dir = info["display_resolution"]
        link_dir(base_path / res_dir, board_path / res_dir, res_dir)

    (board_path / "PUT_YOUR_OGG_FILES_HERE.txt").write_text(
        "将板子专属音效 .ogg 文件放在此目录即可\n"
        "编译时自动发现并嵌入固件，无需手动配置\n",
        encoding="utf-8",
    )
    print("  ✓ 音效目录就绪（放 .ogg 即自动嵌入）")

    print(f"\n✅ 板子文件已创建: {board_path}")
    return board_path


def extend_kconfig_depends(kconfig_path, kconfig_id):
    lines = kconfig_path.read_text(encoding="utf-8").splitlines(keepends=True)
    changed = False
    for i, line in enumerate(lines):
        if "depends on BOARD_TYPE_" not in line:
            continue
        if "BOARD_TYPE_PUPPY" not in line:
            continue
        if kconfig_id in line:
            continue
        lines[i] = line.rstrip("\n") + f" || {kconfig_id}\n"
        changed = True
    if changed:
        kconfig_path.write_text("".join(lines), encoding="utf-8")
        print("  ✓ Kconfig depends 已扩展")


def register_in_build_system(info):
    print("\n🔧 注册到构建系统...")
    kid = info["kconfig_id"]

    kconfig_path = PROJECT_ROOT / "main" / "Kconfig.projbuild"
    kconfig_lines = kconfig_path.read_text(encoding="utf-8").splitlines(keepends=True)
    in_choice = False
    endchoice_idx = None
    for i, line in enumerate(kconfig_lines):
        stripped = line.strip()
        if stripped.startswith("choice BOARD_TYPE"):
            in_choice = True
        elif in_choice and stripped == "endchoice":
            endchoice_idx = i
            break
    if endchoice_idx is not None:
        new_entry = [
            f"    config {kid}\n",
            f'        bool "{info["board_display"]}"\n',
            "        depends on IDF_TARGET_ESP32S3\n",
        ]
        for line in reversed(new_entry):
            kconfig_lines.insert(endchoice_idx, line)
        kconfig_path.write_text("".join(kconfig_lines), encoding="utf-8")
        print(f"  ✓ Kconfig.projbuild 已注册 {kid}")
    else:
        print("  ⚠️  未找到 Kconfig BOARD_TYPE endchoice，请手动添加")

    extend_kconfig_depends(kconfig_path, kid)

    root_cmake = PROJECT_ROOT / "CMakeLists.txt"
    root_lines = root_cmake.read_text(encoding="utf-8").splitlines(keepends=True)
    last_match_idx = None
    for i, line in enumerate(root_lines):
        if 'LINE MATCHES "CONFIG_BOARD_TYPE_' in line:
            last_match_idx = i
    if last_match_idx is not None:
        endif_idx = None
        for i in range(last_match_idx + 1, min(last_match_idx + 8, len(root_lines))):
            if "endif()" in root_lines[i]:
                endif_idx = i
                break
        if endif_idx is not None:
            new_entry = [
                f'        elseif(LINE MATCHES "CONFIG_{kid}=y")\n',
                f'            set(PROJECT_BIN_NAME "{info["project_bin"]}")\n',
                f'            set(BOARD_DIR "{info["board_dir"]}")\n',
            ]
            for line in reversed(new_entry):
                root_lines.insert(endif_idx, line)
            root_cmake.write_text("".join(root_lines), encoding="utf-8")
            print(f"  ✓ 根 CMakeLists.txt 已注册 {info['project_bin']}")
        else:
            print("  ⚠️  未找到根 CMakeLists endif()，请手动添加")
    else:
        print("  ⚠️  未找到根 CMakeLists 板子注册，请手动添加")

    main_cmake = PROJECT_ROOT / "main" / "CMakeLists.txt"
    main_text = main_cmake.read_text(encoding="utf-8")
    tars_marker = "elseif(CONFIG_BOARD_TYPE_TARS)"
    new_block = (
        f'elseif(CONFIG_{kid})\n'
        f'    rig_board("{info["board_dir"]}" "{info["board_display"]}" "{info["project_bin"]}")\n'
    )
    if info.get("has_wakenet") and info.get("wakenet_model") not in (None, "wn9_xiaolutongxue"):
        new_block += (
            f'    set(WAKENET_MODEL "{info["wakenet_model"]}")\n'
            '    set(WAKENET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/wakenet/${WAKENET_MODEL}")\n'
        )
    if tars_marker in main_text:
        main_cmake.write_text(
            main_text.replace(tars_marker, new_block + tars_marker, 1),
            encoding="utf-8",
        )
        print(f"  ✓ main/CMakeLists.txt 已注册 BOARD_DIR={info['board_dir']}（rig_board）")
    else:
        print("  ⚠️  未找到 CONFIG_BOARD_TYPE_TARS 插入点，请手动添加 rig_board() 调用")

    board_cc = BOARDS_DIR / info["board_dir"] / f'{info["board_dir"]}_board.cc'
    print("\n" + "=" * 60)
    print("  ✅ 板子创建完成！")
    print("=" * 60)
    print(f"""
  下一步:
    1. 编辑 {board_cc}
       完善板子逻辑（类名: {info['board_dir'].title()}Board）
    2. 核对 motor.cc：只保留组态，协议用 #include "em3.h" / "scs009.h" / "kp4012.h"
    3. 把专属音效 .ogg 放到板子目录
    4. 独立构建目录编译，避免切板型全量重编:
         idf.py -B build/{info["board_dir"]} -DSDKCONFIG=sdkconfig.{info["board_dir"]} menuconfig
         idf.py -B build/{info["board_dir"]} -DSDKCONFIG=sdkconfig.{info["board_dir"]} build
""")


def main():
    info = collect_board_info()
    create_board_files(info)
    register_in_build_system(info)


if __name__ == "__main__":
    main()
