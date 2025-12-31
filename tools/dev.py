#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.14"
# dependencies = []
# ///
#
# mypy: strict

from __future__ import annotations

import argparse
import asyncio
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Iterable, Mapping, Sequence


@dataclass(frozen=True)
class DevConfig:
    repo_root: Path
    build_dir: Path
    build_type: str


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _quote(argv: Sequence[str]) -> str:
    return " ".join(shlex.quote(a) for a in argv)


async def _pipe(reader: asyncio.StreamReader | None, out: BinaryIO) -> None:
    if reader is None:
        return
    while True:
        chunk = await reader.read(8192)
        if not chunk:
            break
        out.write(chunk)
        out.flush()


async def run_cmd(
    argv: Sequence[str],
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
) -> None:
    print(f"$ {_quote(argv)}", file=sys.stderr)
    proc = await asyncio.create_subprocess_exec(
        *argv,
        cwd=str(cwd),
        env=dict(env) if env is not None else None,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    await asyncio.gather(_pipe(proc.stdout, sys.stdout.buffer), _pipe(proc.stderr, sys.stderr.buffer))
    rc = await proc.wait()
    if rc != 0:
        raise RuntimeError(f"command failed ({rc}): {_quote(argv)}")


def _cmake_configure_args(cfg: DevConfig) -> list[str]:
    return [
        "cmake",
        "-S",
        ".",
        "-B",
        str(cfg.build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={cfg.build_type}",
    ]


def _cmake_build_args(cfg: DevConfig, *, target: str | None = None) -> list[str]:
    argv = ["cmake", "--build", str(cfg.build_dir)]
    if target:
        argv += ["--target", target]
    return argv


def _sandbox_exe(cfg: DevConfig) -> Path:
    return cfg.build_dir / "sandbox"


def _tool_targets() -> list[str]:
    return ["format", "tidy", "cppcheck", "cpplint", "iwyu"]


async def cmd_configure(cfg: DevConfig) -> None:
    await run_cmd(_cmake_configure_args(cfg), cwd=cfg.repo_root)


async def cmd_build(cfg: DevConfig) -> None:
    await run_cmd(_cmake_build_args(cfg), cwd=cfg.repo_root)


async def cmd_test(cfg: DevConfig) -> None:
    await run_cmd(["ctest", "--test-dir", str(cfg.build_dir), "--output-on-failure"], cwd=cfg.repo_root)


async def cmd_run(cfg: DevConfig, args: Sequence[str]) -> None:
    await run_cmd([str(_sandbox_exe(cfg)), *args], cwd=cfg.repo_root)


async def cmd_validate(cfg: DevConfig, *, strict: bool) -> None:
    argv = [str(_sandbox_exe(cfg)), "--validate"]
    if strict:
        argv.append("--strict")
    await run_cmd(argv, cwd=cfg.repo_root)


async def cmd_tools(cfg: DevConfig, targets: Iterable[str]) -> None:
    for t in targets:
        await run_cmd(_cmake_build_args(cfg, target=t), cwd=cfg.repo_root)


async def cmd_check(cfg: DevConfig, *, lint: bool) -> None:
    await cmd_configure(cfg)
    if lint:
        await cmd_tools(cfg, _tool_targets())
    await cmd_build(cfg)
    await cmd_test(cfg)
    await cmd_validate(cfg, strict=True)


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Optional async dev-loop helper (Justfile is the primary workflow).")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="Build directory (default: build)")
    parser.add_argument("--type", default="Debug", help="CMake build type (default: Debug)")

    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("configure", help="Configure the build directory (CMake)")
    sub.add_parser("build", help="Build")
    sub.add_parser("test", help="Run ctest")

    p_run = sub.add_parser("run", help="Run the sandbox binary")
    p_run.add_argument("args", nargs=argparse.REMAINDER, help="Args passed to the sandbox binary")

    p_val = sub.add_parser("validate", help="Run --validate")
    p_val.add_argument("--strict", action="store_true", help="Treat warnings as errors")

    p_tools = sub.add_parser("tools", help="Run one or more CMake devtools targets")
    p_tools.add_argument("targets", nargs="*", choices=_tool_targets(), help="Subset to run (default: all)")

    p_check = sub.add_parser("check", help="Configure -> (optional lint) -> build -> test -> validate --strict")
    p_check.add_argument("--lint", action="store_true", help="Also run format/tidy/cppcheck/cpplint/iwyu")

    return parser.parse_args(argv)


async def _main_async(ns: argparse.Namespace) -> int:
    cfg = DevConfig(repo_root=_repo_root(), build_dir=ns.build_dir, build_type=str(ns.type))

    try:
        match str(ns.cmd):
            case "configure":
                await cmd_configure(cfg)
            case "build":
                await cmd_configure(cfg)
                await cmd_build(cfg)
            case "test":
                await cmd_configure(cfg)
                await cmd_build(cfg)
                await cmd_test(cfg)
            case "run":
                await cmd_configure(cfg)
                await cmd_build(cfg)
                args = list(ns.args)
                if args and args[0] == "--":
                    args = args[1:]
                await cmd_run(cfg, args)
            case "validate":
                await cmd_configure(cfg)
                await cmd_build(cfg)
                await cmd_validate(cfg, strict=bool(ns.strict))
            case "tools":
                await cmd_configure(cfg)
                targets = list(ns.targets) if ns.targets else _tool_targets()
                await cmd_tools(cfg, targets)
            case "check":
                await cmd_check(cfg, lint=bool(ns.lint))
            case _:
                print(f"unknown command: {ns.cmd}", file=sys.stderr)
                return 2
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130

    return 0


def main(argv: list[str]) -> int:
    ns = _parse(argv)
    return asyncio.run(_main_async(ns))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
