#!/usr/bin/env python3
"""Upload already-packaged RIG firmware to the Luwu-Tools server.

Only files referenced by the selected boards' manifests are transferred.
The entire live bin tree is backed up before every upload (and before a
rollback). Firmware is uploaded before the new manifests are published.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

from package_luwu_firmware import (
    BOARD_LABELS,
    PROJECT_ROOT,
    PackageError,
    error,
    interactive_select,
    ok,
    parse_boards,
    tail_log,
)


MANIFESTS = ("manifest.json", "manifest_app.json", "manifest_assets.json")
SAFE_HOST = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
SAFE_REMOTE_PATH = re.compile(r"^/[A-Za-z0-9_./-]+$")
BACKUP_ID = re.compile(r"^bin-[0-9]{8}T[0-9]{12}Z$")


def validate_destination(host: str, user: str, remote_dir: str) -> None:
    if not SAFE_HOST.fullmatch(host) or host.startswith("-"):
        raise PackageError("SSH 主机名/IP 不合法；请用 --host 指定，不要包含用户名")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_-]*", user):
        raise PackageError("SSH 用户名不合法")
    if not SAFE_REMOTE_PATH.fullmatch(remote_dir) or ".." in PurePosixPath(remote_dir).parts:
        raise PackageError("远端 bin 路径必须是无空格、无 .. 的绝对路径")
    if remote_dir == "/":
        raise PackageError("远端 bin 路径不能是根目录")


def validate_backup_dir(remote_dir: str, backup_dir: str) -> None:
    if not SAFE_REMOTE_PATH.fullmatch(backup_dir) or ".." in PurePosixPath(backup_dir).parts:
        raise PackageError("备份目录必须是无空格、无 .. 的绝对路径")
    live, backups = PurePosixPath(remote_dir), PurePosixPath(backup_dir)
    if live.is_relative_to(backups) or backups.is_relative_to(live):
        raise PackageError("备份目录不能与 bin 目录重叠")


def new_backup_id() -> str:
    return "bin-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def backup_command(remote_dir: str, backup_dir: str, backup_id: str) -> str:
    """Copy all live bin files into a new, complete snapshot on the server."""
    if not BACKUP_ID.fullmatch(backup_id):
        raise PackageError("备份编号无效")
    source = shlex.quote(remote_dir.rstrip("/"))
    root = shlex.quote(backup_dir.rstrip("/"))
    temporary = shlex.quote(f"{backup_dir.rstrip('/')}/.{backup_id}.incomplete")
    final = shlex.quote(f"{backup_dir.rstrip('/')}/{backup_id}")
    return (
        f"set -eu; umask 077; test -d {source}; test ! -L {source}; "
        f"mkdir -p {root}; test ! -L {root}; test ! -e {temporary}; test ! -e {final}; "
        f"mkdir {temporary}; cp -a {source}/. {temporary}/; "
        f"mv {temporary} {final}"
    )


def restore_command(remote_dir: str, backup_dir: str, backup_id: str) -> str:
    """Restore an exact snapshot (including removal of files added later)."""
    if not BACKUP_ID.fullmatch(backup_id):
        raise PackageError("备份编号无效")
    source = shlex.quote(f"{backup_dir.rstrip('/')}/{backup_id}")
    destination = shlex.quote(remote_dir.rstrip("/"))
    return (
        f"set -eu; test -d {source}; test ! -L {source}; test -d {destination}; "
        f"rsync -ac --delete --delay-updates {source}/ {destination}/"
    )


def list_command(backup_dir: str) -> str:
    root = shlex.quote(backup_dir.rstrip("/"))
    return f"test -d {root} && find {root} -mindepth 1 -maxdepth 1 -type d -name 'bin-*' -printf '%f\\n'"


def package_dirs(boards: list[str]) -> list[tuple[Path, tuple[str, ...]]]:
    result = []
    for board in boards:
        result.extend(
            [
                (Path("RIG") / board, MANIFESTS),
                (Path("RIG") / board / "gb", MANIFESTS),
                (Path("GARAGE") / board, ("manifest.json",)),
            ]
        )
    return result


def collect_files(bin_root: Path, boards: list[str]) -> tuple[list[Path], list[Path]]:
    firmware: set[Path] = set()
    manifests: set[Path] = set()
    for directory, names in package_dirs(boards):
        for name in names:
            relative_manifest = directory / name
            manifest_path = bin_root / relative_manifest
            if not manifest_path.is_file() or manifest_path.is_symlink():
                raise PackageError(f"缺少固件清单：{manifest_path}")
            try:
                data = json.loads(manifest_path.read_text(encoding="utf-8"))
                builds = data["builds"]
                if not isinstance(builds, list) or not builds:
                    raise ValueError("builds 为空")
                parts = [part for build in builds for part in build["parts"]]
                if not parts:
                    raise ValueError("parts 为空")
            except (OSError, ValueError, KeyError, TypeError) as exc:
                raise PackageError(f"清单无效：{manifest_path}（{exc}）") from exc
            manifests.add(relative_manifest)
            for part in parts:
                try:
                    filename = part["path"]
                except (KeyError, TypeError) as exc:
                    raise PackageError(f"清单缺少固件路径：{manifest_path}") from exc
                if not isinstance(filename, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+\.bin", filename):
                    raise PackageError(f"清单包含不安全的固件路径：{manifest_path}: {filename!r}")
                relative_file = directory / filename
                source = bin_root / relative_file
                if not source.is_file() or source.is_symlink() or source.stat().st_size == 0:
                    raise PackageError(f"固件缺失或为空：{source}")
                firmware.add(relative_file)
    return sorted(firmware), sorted(manifests)


def run_logged(command: list[str], log, label: str) -> None:
    log.write(f"\n{label}\ncommand: {shlex.join(command)}\n")
    log.flush()
    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise PackageError(f"{label}失败（退出码 {result.returncode}）")


def capture_logged(command: list[str], log, label: str) -> str:
    log.write(f"\n{label}\ncommand: {shlex.join(command)}\n")
    log.flush()
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
    log.write(result.stdout)
    log.flush()
    if result.returncode:
        raise PackageError(f"{label}失败（退出码 {result.returncode}）")
    return result.stdout


def upload_batch(
    files: list[Path], bin_root: Path, target: str, ssh_command: list[str], log, label: str
) -> None:
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", prefix="rig-upload-", delete=True) as file_list:
        file_list.write("\n".join(path.as_posix() for path in files) + "\n")
        file_list.flush()
        command = [
            "rsync", "-azc", "--delay-updates", "--files-from", file_list.name,
            "-e", shlex.join(ssh_command), str(bin_root) + "/", target + "/",
        ]
        run_logged(command, log, label)


def main() -> int:
    parser = argparse.ArgumentParser(description="上传已打包的 RIG 固件到 Luwu-Tools 服务器")
    parser.add_argument("-b", "--board", help="puppy/hover/arm/tars/all；省略时交互选择")
    parser.add_argument(
        "-o", "--output", default=str(PROJECT_ROOT / "releases" / "Luwu-Tools"),
        help="打包输出根目录（包含 bin/）",
    )
    parser.add_argument("--host", default=os.environ.get("LUWU_TOOLS_SSH_HOST"), help="SSH 主机名/IP")
    parser.add_argument("--user", default=os.environ.get("LUWU_TOOLS_SSH_USER", "root"), help="SSH 用户名，默认 root")
    parser.add_argument("--port", type=int, default=22, help="SSH 端口，默认 22")
    parser.add_argument("--identity-file", help="SSH 私钥路径；默认使用 SSH 自身配置")
    parser.add_argument("--remote-dir", default="/root/Luwu-Tools/bin", help="服务器上的 bin 目录")
    parser.add_argument("--backup-dir", help="服务器上的备份根目录，默认 bin 同级的 backups/")
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--list-backups", action="store_true", help="列出服务器上的备份编号")
    action.add_argument("--rollback", metavar="BACKUP_ID", help="回退到指定备份；操作前会再备份当前 bin")
    parser.add_argument("--dry-run", action="store_true", help="只显示本地计划，不连接服务器")
    args = parser.parse_args()

    try:
        if not args.host:
            raise PackageError("请用 --host 指定服务器 IP/域名，或设置 LUWU_TOOLS_SSH_HOST")
        validate_destination(args.host, args.user, args.remote_dir)
        backup_dir = args.backup_dir or str(PurePosixPath(args.remote_dir).parent / "backups")
        validate_backup_dir(args.remote_dir, backup_dir)
        if not 1 <= args.port <= 65535:
            raise PackageError("SSH 端口必须在 1–65535 之间")
        identity = Path(args.identity_file).expanduser().resolve() if args.identity_file else None
        if identity and not identity.is_file():
            raise PackageError(f"SSH 私钥不存在：{identity}")
        if args.rollback and not BACKUP_ID.fullmatch(args.rollback):
            raise PackageError("备份编号无效；请通过 --list-backups 查看")
        if (args.list_backups or args.rollback) and args.board:
            raise PackageError("列出备份/回退的是完整 bin 目录，无需指定 --board")

        boards = [] if args.list_backups or args.rollback else (
            parse_boards(args.board) if args.board else interactive_select()
        )
        bin_root = Path(args.output).expanduser().resolve() / "bin"
        firmware, manifests = collect_files(bin_root, boards) if boards else ([], [])
        target = f"{args.user}@{args.host}:{args.remote_dir.rstrip('/')}"
        print(f"远端：{target}")
        print(f"备份目录：{backup_dir}")
        if boards:
            print(f"机器人：{', '.join(BOARD_LABELS[b] for b in boards)}")
            print(f"本地：{bin_root}")
            print(f"文件：{len(firmware)} 个固件 + {len(manifests)} 个清单")
        if args.dry_run:
            if args.rollback:
                print(f"将先备份当前完整 bin，再回退到：{args.rollback}（回退会删除备份中不存在的线上文件）")
            elif args.list_backups:
                print("将列出服务器上的备份（dry-run 不连接服务器）")
            else:
                print("将先备份服务器上的完整 bin，再上传：")
                for path in firmware + manifests:
                    print(f"  {path.as_posix()}")
            ok("检查完成（dry-run，未连接服务器）")
            return 0

        if not shutil.which("ssh") or not shutil.which("rsync"):
            raise PackageError("需要安装 ssh 和 rsync")
        ssh_command = ["ssh", "-p", str(args.port), "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes"]
        if identity:
            ssh_command.extend(["-i", str(identity)])
        log_dir = PROJECT_ROOT / "releases" / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        log_path = log_dir / f"upload-{datetime.now().strftime('%Y%m%d-%H%M%S-%f')}.log"
        print(f"日志：{log_path}")
        with log_path.open("w", encoding="utf-8") as log:
            log.write(f"RIG deployment {datetime.now().isoformat()}\nsource={bin_root}\ntarget={target}\nbackup_dir={backup_dir}\n")
            ssh_target = f"{args.user}@{args.host}"
            run_logged(
                ssh_command + [ssh_target, f"test -d {shlex.quote(args.remote_dir.rstrip('/'))}"],
                log, "检查远端 bin 目录和 SSH 连接",
            )
            if args.list_backups:
                result = capture_logged(ssh_command + [ssh_target, list_command(backup_dir)], log, "列出备份")
                print("\n".join(sorted(result.splitlines())) or "暂无备份")
                return 0

            if args.rollback:
                source = shlex.quote(f"{backup_dir.rstrip('/')}/{args.rollback}")
                run_logged(
                    ssh_command + [ssh_target, f"test -d {source} && test ! -L {source}"],
                    log, "检查待回退的备份",
                )

            backup_id = new_backup_id()
            print(f"备份当前完整 bin：{backup_id}…", flush=True)
            run_logged(
                ssh_command + [ssh_target, backup_command(args.remote_dir, backup_dir, backup_id)],
                log, "备份服务器 bin",
            )
            print(f"备份完成：{backup_dir}/{backup_id}")
            if args.rollback:
                print(f"回退到 {args.rollback}…", flush=True)
                run_logged(
                    ssh_command + [ssh_target, restore_command(args.remote_dir, backup_dir, args.rollback)],
                    log, "回退服务器 bin",
                )
                ok(f"回退完成；操作前的版本保存在 {backup_dir}/{backup_id}")
            else:
                print("上传固件文件…", flush=True)
                upload_batch(firmware, bin_root, target, ssh_command, log, "上传固件文件")
                print("发布清单…", flush=True)
                upload_batch(manifests, bin_root, target, ssh_command, log, "发布清单")
                ok(f"上传完成；原版保存在 {backup_dir}/{backup_id}")
        return 0
    except (PackageError, OSError) as exc:
        error(f"上传失败：{exc}")
        if "log_path" in locals():
            error(f"日志：{log_path}")
            excerpt = tail_log(log_path)
            if excerpt:
                error("日志末尾：\n" + excerpt)
        return 1
    except KeyboardInterrupt:
        error("上传已取消")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
