#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.14"
# dependencies = []
# ///

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


SHEETS: dict[str, str] = {
    "x": "https://www.spriters-resource.com/media/assets/2/1224.png?updated=1755472404",
    "zero": "https://www.spriters-resource.com/media/assets/3/3082.png?updated=1755472413",
    "font": "https://www.spriters-resource.com/media/assets/134/137131.png?updated=1755481029",
}


def default_out_dir() -> Path:
    repo_root = Path(__file__).resolve().parents[1]
    return repo_root / "assets" / "sprites" / "external"


def download(url: str, dest: Path) -> None:
    req = Request(url, headers={"User-Agent": "sdl3-sandbox (uv script)"})
    with urlopen(req) as resp:
        dest.parent.mkdir(parents=True, exist_ok=True)
        with dest.open("wb") as f:
            shutil.copyfileobj(resp, f)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Download a few Spriters Resource sheets for local testing (not committed).")
    parser.add_argument("--out", type=Path, default=default_out_dir(), help="Output directory (default: assets/sprites/external)")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing files")
    parser.add_argument("names", nargs="*", choices=sorted(SHEETS.keys()), help="Subset to download (default: all)")
    args = parser.parse_args(argv)

    out_dir: Path = args.out
    names: list[str] = list(args.names) if args.names else list(SHEETS.keys())

    out_dir.mkdir(parents=True, exist_ok=True)

    for name in names:
        url = SHEETS[name]
        dest = out_dir / f"{name}_sheet.png"

        if dest.exists() and not args.overwrite:
            print(f"Skip (exists): {dest}")
            continue

        try:
            download(url, dest)
        except (HTTPError, URLError) as e:
            print(f"Failed: {name} ({url}) -> {dest}\n  {e}", file=sys.stderr)
            return 1

        print(f"Wrote: {dest}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
