#!/usr/bin/env python3
"""Build RIG firmware packages using the Luwu-Tools directory layout.

The script builds both domestic (cn) and international (gb) firmware for the
selected robots, builds the unified hardware-test firmware once, and produces:

    bin/RIG/<board>/...       domestic firmware
    bin/RIG/<board>/gb/...    international firmware
    bin/GARAGE/<board>/...    unified hardware-test firmware

Build output is written to a timestamped log.  The terminal only shows compact
progress and a red error summary when a command fails.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import traceback
from datetime import datetime


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SUPPORTED_BOARDS = ("puppy", "hover", "arm", "tars")
BOARD_LABELS = {
    "puppy": "Puppy",
    "hover": "Hover",
    "arm": "ARM",
    "tars": "TARS",
}
APP_NAMES = {board: f"rig-{board}.bin" for board in SUPPORTED_BOARDS}
REGIONS = (("cn", "国内版"), ("gb", "国际版"))

OFFSETS = {
    "bootloader": 0x0000,
    "partition": 0x8000,
    "app": 0x20000,
    "assets": 0x800000,
    "test_app": 0x10000,
}


class PackageError(RuntimeError):
    """Expected packaging failure with a user-facing message."""


def color(text: str, code: str) -> str:
    if not sys.stderr.isatty() or os.environ.get("NO_COLOR"):
        return text
    return f"\033[{code}m{text}\033[0m"


def ok(text: str) -> None:
    print(color(text, "32"))


def error(text: str) -> None:
    print(color(text, "31"), file=sys.stderr)


def parse_boards(value: str) -> list[str]:
    normalized = value.strip().lower().replace("，", ",")
    if normalized in {"all", "全部", "5"}:
        return list(SUPPORTED_BOARDS)

    numeric = {"1": "puppy", "2": "hover", "3": "arm", "4": "tars"}
    boards: list[str] = []
    for token in re.split(r"[,\s]+", normalized):
        if not token:
            continue
        board = numeric.get(token, token)
        if board not in SUPPORTED_BOARDS:
            raise PackageError(f"不支持的机器人：{token}")
        if board not in boards:
            boards.append(board)
    if not boards:
        raise PackageError("没有选择机器人")
    return boards


def interactive_select() -> list[str]:
    print("请选择要打包的机器人：")
    print("  1. Puppy")
    print("  2. Hover")
    print("  3. ARM")
    print("  4. TARS")
    print("  5. 全部")
    print("可输入单项、多个编号（如 1,3）或 all。")
    while True:
        try:
            return parse_boards(input("选择："))
        except PackageError as exc:
            error(f"错误：{exc}")


def discover_idf_path(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit).expanduser())
    if os.environ.get("IDF_PATH"):
        candidates.append(Path(os.environ["IDF_PATH"]).expanduser())

    espressif_root = Path.home() / ".espressif"
    # The project targets ESP-IDF 5.5.x. Prefer the newest installed 5.5
    # release before considering other locally installed IDF versions.
    preferred = sorted(espressif_root.glob("v5.5*/esp-idf"), reverse=True)
    candidates.extend(preferred)
    candidates.extend(
        candidate
        for candidate in sorted(espressif_root.glob("v*/esp-idf"), reverse=True)
        if candidate not in preferred
    )
    candidates.append(espressif_root / "esp-idf")

    for candidate in candidates:
        if (candidate / "export.sh").is_file() and (candidate / "tools" / "idf.py").is_file():
            return candidate.resolve()
    raise PackageError("未找到 ESP-IDF。请先加载 ESP-IDF，或使用 --idf-path 指定路径")


def load_idf_environment(idf_path: Path, log) -> dict[str, str]:
    shell = os.environ.get("SHELL", "/bin/zsh")
    command = f"source {shlex.quote(str(idf_path / 'export.sh'))} >/dev/null && env -0"
    proc = subprocess.run(
        [shell, "-lc", command],
        stdout=subprocess.PIPE,
        stderr=log,
        check=False,
    )
    if proc.returncode != 0:
        raise PackageError("ESP-IDF 环境加载失败")

    env: dict[str, str] = {}
    for entry in proc.stdout.split(b"\0"):
        if b"=" not in entry:
            continue
        key, value = entry.split(b"=", 1)
        env[key.decode(errors="replace")] = value.decode(errors="replace")
    env["IDF_PATH"] = str(idf_path)
    return env


def run_logged(command: list[str], cwd: Path, env: dict[str, str], log, label: str) -> None:
    log.write(f"\n{'=' * 78}\n{label}\n")
    log.write(f"cwd: {cwd}\ncommand: {shlex.join(command)}\n{'-' * 78}\n")
    log.flush()
    proc = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        stdout=log,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if proc.returncode != 0:
        raise PackageError(f"{label}失败（退出码 {proc.returncode}）")


def write_sdkconfig_seed(path: Path, board: str, region: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        'CONFIG_IDF_TARGET="esp32s3"',
        "CONFIG_IDF_TARGET_ESP32S3=y",
        "",
    ]
    for candidate in SUPPORTED_BOARDS:
        symbol = f"CONFIG_BOARD_TYPE_{candidate.upper()}"
        lines.append(f"{symbol}=y" if candidate == board else f"# {symbol} is not set")
    lines.append("")
    lines.extend(
        [
            "CONFIG_FIRMWARE_REGION_DOMESTIC=y"
            if region == "cn"
            else "# CONFIG_FIRMWARE_REGION_DOMESTIC is not set",
            "CONFIG_FIRMWARE_REGION_OVERSEAS=y"
            if region == "gb"
            else "# CONFIG_FIRMWARE_REGION_OVERSEAS is not set",
            "CONFIG_SR_WN_WN9_HEYKIRA_TTS3=y"
            if region == "gb"
            else "# CONFIG_SR_WN_WN9_HEYKIRA_TTS3 is not set",
            "CONFIG_FLASH_EXPRESSION_ASSETS=y",
            "CONFIG_USE_EMOTE_MESSAGE_STYLE=y",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def require_file(path: Path, description: str) -> Path:
    if not path.is_file():
        raise PackageError(f"缺少{description}：{path}")
    return path


def read_board_version(board: str, build_dir: Path) -> str:
    version_file = PROJECT_ROOT / "main" / "boards" / board / "version.txt"
    if version_file.is_file():
        version = version_file.read_text(encoding="utf-8").strip()
        if version:
            return version

    description = build_dir / "project_description.json"
    if description.is_file():
        version = json.loads(description.read_text(encoding="utf-8")).get("version")
        if version:
            return str(version)
    raise PackageError(f"无法确定 {BOARD_LABELS[board]} 的固件版本")


def read_test_version(test_root: Path) -> str:
    main_file = test_root / "main" / "main.c"
    text = require_file(main_file, "测试固件版本文件").read_text(encoding="utf-8")
    match = re.search(r'#define\s+TEST_FW_VERSION\s+"([^"]+)"', text)
    if not match:
        raise PackageError("无法从测试固件读取 TEST_FW_VERSION")
    return match.group(1)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def firmware_manifest(board: str, version: str, mode: str) -> dict:
    label = BOARD_LABELS[board]
    if mode == "full":
        name = f"RIG {label}固件"
        parts = [
            {"path": "bootloader.bin", "offset": OFFSETS["bootloader"]},
            {"path": "partition-table.bin", "offset": OFFSETS["partition"]},
            {"path": APP_NAMES[board], "offset": OFFSETS["app"]},
            {"path": "assets.bin", "offset": OFFSETS["assets"]},
        ]
    elif mode == "app":
        name = f"RIG {label}应用固件"
        parts = [{"path": APP_NAMES[board], "offset": OFFSETS["app"]}]
    else:
        name = f"RIG {label}资源文件"
        parts = [{"path": "assets.bin", "offset": OFFSETS["assets"]}]
    return {
        "name": name,
        "version": version,
        "home_assistant_domain": "",
        "builds": [{"chipFamily": "ESP32-S3", "improv": False, "parts": parts}],
    }


def test_manifest(board: str, version: str) -> dict:
    return {
        "name": f"RIG 统一测试固件 [{BOARD_LABELS[board].upper()}]",
        "version": version,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "improv": False,
                "parts": [
                    {"path": "bootloader.bin", "offset": OFFSETS["bootloader"]},
                    {"path": "partition-table.bin", "offset": OFFSETS["partition"]},
                    {"path": "rig-hw-test.bin", "offset": OFFSETS["test_app"]},
                ],
            }
        ],
    }


def build_main_firmware(
    board: str,
    region: str,
    env: dict[str, str],
    idf_py: str,
    log,
) -> tuple[Path, str]:
    build_dir = PROJECT_ROOT / "build" / "packages" / f"{board}-{region}"
    config_file = PROJECT_ROOT / "build" / "package-configs" / f"sdkconfig.{board}-{region}"
    write_sdkconfig_seed(config_file, board, region)

    defaults = ";".join(
        str(path)
        for path in (PROJECT_ROOT / "sdkconfig.defaults", PROJECT_ROOT / "sdkconfig.defaults.esp32s3")
    )
    command = [
        idf_py,
        "-B",
        str(build_dir),
        f"-DSDKCONFIG={config_file}",
        f"-DSDKCONFIG_DEFAULTS={defaults}",
        "-DIDF_TARGET=esp32s3",
        "build",
    ]
    run_logged(command, PROJECT_ROOT, env, log, f"构建 {BOARD_LABELS[board]} {region}")
    return build_dir, read_board_version(board, build_dir)


def build_test_firmware(env: dict[str, str], idf_py: str, log) -> tuple[Path, str]:
    test_root = PROJECT_ROOT / "Test_Firmware"
    if not test_root.is_dir():
        test_root = PROJECT_ROOT / "test_firmware"
    if not test_root.is_dir():
        raise PackageError("未找到 Test_Firmware 测试固件目录")

    build_dir = test_root / "build-package"
    run_logged(
        [idf_py, "-B", str(build_dir), "build"],
        test_root,
        env,
        log,
        "构建统一测试固件",
    )
    return build_dir, read_test_version(test_root)


def stage_firmware(stage: Path, board: str, region: str, build_dir: Path, version: str) -> None:
    destination = stage / "bin" / "RIG" / board
    if region == "gb":
        destination /= "gb"
    destination.mkdir(parents=True, exist_ok=True)

    sources = {
        "bootloader.bin": require_file(build_dir / "bootloader" / "bootloader.bin", "bootloader"),
        "partition-table.bin": require_file(
            build_dir / "partition_table" / "partition-table.bin", "分区表"
        ),
        APP_NAMES[board]: require_file(build_dir / APP_NAMES[board], "应用固件"),
        "assets.bin": require_file(build_dir / "board_assets.bin", "资源固件"),
    }
    for name, source in sources.items():
        shutil.copy2(source, destination / name)

    write_json(destination / "manifest.json", firmware_manifest(board, version, "full"))
    write_json(destination / "manifest_app.json", firmware_manifest(board, version, "app"))
    write_json(destination / "manifest_assets.json", firmware_manifest(board, version, "assets"))


def stage_test_firmware(stage: Path, boards: list[str], build_dir: Path, version: str) -> None:
    sources = {
        "bootloader.bin": require_file(build_dir / "bootloader" / "bootloader.bin", "测试 bootloader"),
        "partition-table.bin": require_file(
            build_dir / "partition_table" / "partition-table.bin", "测试分区表"
        ),
        "rig-hw-test.bin": require_file(build_dir / "rig_hw_test.bin", "测试应用固件"),
    }
    for board in boards:
        destination = stage / "bin" / "GARAGE" / board
        destination.mkdir(parents=True, exist_ok=True)
        for name, source in sources.items():
            shutil.copy2(source, destination / name)
        write_json(destination / "manifest.json", test_manifest(board, version))


def publish(stage: Path, output: Path) -> None:
    for source in stage.rglob("*"):
        if not source.is_file():
            continue
        relative = source.relative_to(stage)
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(f".{destination.name}.tmp-{os.getpid()}")
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)


def tail_log(path: Path, count: int = 18) -> str:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        return "\n".join(lines[-count:])
    except OSError:
        return ""


def main() -> int:
    parser = argparse.ArgumentParser(description="静默构建并打包 RIG 固件到 Luwu-Tools 目录结构")
    parser.add_argument(
        "-b",
        "--board",
        help="puppy/hover/arm/tars/all；多个机器人用逗号分隔。省略时交互选择",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=str(PROJECT_ROOT / "releases" / "Luwu-Tools"),
        help="输出根目录（其下生成 bin/RIG 和 bin/GARAGE）",
    )
    parser.add_argument("--idf-path", help="ESP-IDF 根目录；默认从环境和 ~/.espressif 自动发现")
    parser.add_argument("--dry-run", action="store_true", help="只显示构建计划，不执行构建")
    args = parser.parse_args()

    try:
        boards = parse_boards(args.board) if args.board else interactive_select()
        output = Path(args.output).expanduser().resolve()
        log_dir = PROJECT_ROOT / "releases" / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        log_path = log_dir / f"package-{timestamp}.log"

        print(f"机器人：{', '.join(BOARD_LABELS[b] for b in boards)}")
        print("版本：国内版 + 国际版 + 统一测试固件")
        print(f"输出：{output}")
        print(f"日志：{log_path}")
        if args.dry_run:
            ok("检查完成（dry-run，未执行构建）")
            return 0

        idf_path = discover_idf_path(args.idf_path)
        with log_path.open("w", encoding="utf-8") as log:
            log.write(f"RIG firmware package log {datetime.now().isoformat()}\n")
            log.write(f"boards={','.join(boards)}\noutput={output}\nidf={idf_path}\n")
            env = load_idf_environment(idf_path, log)
            idf_py = shutil.which("idf.py", path=env.get("PATH"))
            if not idf_py:
                idf_py = str(idf_path / "tools" / "idf.py")

            with tempfile.TemporaryDirectory(prefix="rig-package-") as temp_dir:
                stage = Path(temp_dir)
                for board in boards:
                    for region, region_label in REGIONS:
                        print(f"构建 {BOARD_LABELS[board]} {region_label}…", flush=True)
                        build_dir, version = build_main_firmware(board, region, env, idf_py, log)
                        stage_firmware(stage, board, region, build_dir, version)
                        ok(f"  完成：{BOARD_LABELS[board]} {region_label} v{version}")

                print("构建统一测试固件…", flush=True)
                test_build_dir, test_version = build_test_firmware(env, idf_py, log)
                stage_test_firmware(stage, boards, test_build_dir, test_version)
                ok(f"  完成：统一测试固件 v{test_version}")

                publish(stage, output)

        ok("打包完成")
        print(f"产物：{output / 'bin'}")
        print(f"日志：{log_path}")
        return 0
    except (PackageError, OSError, json.JSONDecodeError) as exc:
        error(f"打包失败：{exc}")
        if "log_path" in locals():
            error(f"日志：{log_path}")
            excerpt = tail_log(log_path)
            if excerpt:
                error("日志末尾：\n" + excerpt)
        return 1
    except KeyboardInterrupt:
        error("打包已取消")
        return 130
    except Exception as exc:
        error(f"打包失败：未预期错误：{exc}")
        if "log_path" in locals():
            try:
                with log_path.open("a", encoding="utf-8") as log:
                    log.write("\nUNEXPECTED ERROR\n")
                    log.write(traceback.format_exc())
            except OSError:
                pass
            error(f"日志：{log_path}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
