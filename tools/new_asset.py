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
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import cast


_ID_RE = re.compile(r"^[a-z0-9_]+$")


@dataclass(frozen=True)
class WriteOptions:
    repo_root: Path
    overwrite: bool


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _validate_id(raw: str) -> str:
    s = raw.strip().lower()
    if not s:
        raise ValueError("id must be non-empty")
    if not _ID_RE.fullmatch(s):
        raise ValueError("id must match [a-z0-9_]+ (lowercase)")
    return s


def _default_display(asset_id: str) -> str:
    # "mega_man_x" -> "Mega Man X"
    parts = asset_id.split("_")
    return " ".join(p.upper() if len(p) == 1 else p.capitalize() for p in parts)


def _write_text(path: Path, text: str, opts: WriteOptions) -> None:
    if path.exists() and not opts.overwrite:
        raise FileExistsError(f"exists: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def _toml_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def _character_template(asset_id: str, display: str) -> str:
    sheet = f"assets/sprites/{asset_id}_placeholder.bmp"
    return f"""version = 1

[name]
id = "{asset_id}"
display = "{_toml_escape(display)}"

[render]
sheet = "{sheet}"
frame_w = 32
frame_h = 32
scale = 1.0

# render.anims defaults to idle/run/jump/fall/dash/glide/spindash_charge.
# Uncomment to override (example):
# [render.anims.dash]
# rotate_deg = 45.0

[collision]
w = 24.0
h = 32.0

[physics]
gravity = 0.166667
max_fall_speed = 18.33
ground_friction = 5.0
air_drag = 0.8

[move]
model = "approach"
air_control = true
accel_ground = 0.166667
accel_air = 0.097222
decel_ground = 0.194444
max_speed_ground = 3.0
max_speed_air = 2.83
turn_resistance = 0.20

# Optional per-zone environment overrides (multiplies stage zones).
# [environment.water]
# max_speed_multiplier = 1.0
# accel_multiplier = 1.0
#
# [environment.ice]
# friction_multiplier = 1.0

[math]
subpixel = 256
collision_snap = "pixel"
quantize = "trunc"

[jump]
enabled = true
model = "impulse"
impulse = 4.0
coyote_frames = 11
jump_buffer_frames = 11
max_hold_frames = 0
variable_jump = true
variable_cut_multiplier = 0.45
fall_gravity_multiplier = 1.0
rise_gravity_multiplier_held = 1.0
rise_gravity_multiplier_released = 1.0
release_clamp_vy = 0.0

# Optional jump impulse table vs abs(vx) (SMW/SMB3-ish).
# Entries are applied in ascending min_abs_vx; last match wins.
# [[jump.impulse_by_speed]]
# min_abs_vx = 0.0   # px/s
# impulse = 900.0    # px/s

[actions.dash]
enabled = false
type = "press"
input = "action1"
hold_frames = 0
cooldown_frames = 0
dash_speed = 6.0
dash_time_frames = 22
allow_air = true
air_dashes = -1

[actions.spindash]
enabled = false
type = "hold"
input = "action1"
require_down = false
down_window_frames = 0
charge_frames = 60
min_launch_speed = 3.50
max_launch_speed = 6.0
tap_boost_frames = 0

[actions.glide]
enabled = false
type = "hold"
input = "action2"
start_on_press = false
gravity_multiplier = 0.35
max_fall_speed = 4.17

[actions.spin]
enabled = false
type = "hold"
input = "action1"
min_speed = 2.0
spin_friction = 4.5

[actions.fly]
enabled = false
type = "hold"
input = "action2"
up_accel = 0.194444
max_up_speed = 2.17

[states.drop_through]
hold_frames = 22
"""


def _stage_template(asset_id: str, display: str) -> str:
    return f"""version = 1

[stage]
id = "{asset_id}"
display = "{_toml_escape(display)}"

[world]
width = 2000.0
height = 600.0

[[solids]]
x = 0.0
y = 500.0
w = 2000.0
h = 80.0

# Optional environment zones (water/ice/etc).
# [[zones]]
# x = 800.0
# y = 0.0
# w = 800.0
# h = 600.0
# type = "water"
# gravity_multiplier = 1.0
# max_fall_speed_multiplier = 1.0
# accel_multiplier = 0.5
# max_speed_multiplier = 0.5
# friction_multiplier = 1.0
# ground_friction_multiplier = 1.0
# air_drag_multiplier = 1.0
# turn_resistance_multiplier = 1.0
# jump_impulse_multiplier = 1.0

[spawns.default]
x = 120.0
y = 120.0
facing = 1

[camera]
deadzone_w = 240.0
deadzone_h = 140.0
lookahead_x = 120.0
lookahead_y = 0.0
look_hold_ms = 0
look_up_y = 0.0
look_down_y = 0.0
no_backscroll = false

[camera.bounds]
x = 0.0
y = 0.0
w = 2000.0
h = 540.0

# Optional boss-room style camera locks (strict clamp while player overlaps trigger rect).
# [[camera.locks]]
# x = 1500.0
# y = 0.0
# w = 1200.0
# h = 600.0
# bounds_x = 1500.0
# bounds_y = 0.0
# bounds_w = 1200.0
# bounds_h = 540.0

# Optional hazards (minimal “combat-ish” scaffold).
# [[hazards]]
# x = 400.0
# y = 0.0
# w = 80.0
# h = 600.0
# iframes_ms = 800
# ignore_iframes = false
# lockout_ms = 0
# knockback_vx = 700.0
# knockback_vy = 0.0

[collision]
ground_snap = 2.0
step_up = 6.0
skin = 0.01
"""


def _cmd_character(args: argparse.Namespace, opts: WriteOptions) -> int:
    try:
        asset_id = _validate_id(str(args.id))
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    display = str(args.display) if args.display else _default_display(asset_id)
    out_path = opts.repo_root / "assets" / "characters" / f"{asset_id}.toml"
    text = _character_template(asset_id, display)

    try:
        _write_text(out_path, text, opts)
    except FileExistsError as e:
        print(f"error: {e} (use --overwrite)", file=sys.stderr)
        return 2

    print(f"Wrote: {out_path}")
    return 0


def _cmd_stage(args: argparse.Namespace, opts: WriteOptions) -> int:
    try:
        asset_id = _validate_id(str(args.id))
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    display = str(args.display) if args.display else _default_display(asset_id)
    out_path = opts.repo_root / "assets" / "stages" / f"{asset_id}.toml"
    text = _stage_template(asset_id, display)

    try:
        _write_text(out_path, text, opts)
    except FileExistsError as e:
        print(f"error: {e} (use --overwrite)", file=sys.stderr)
        return 2

    print(f"Wrote: {out_path}")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Scaffold stage/character TOML assets (versioned).")
    parser.add_argument("--repo-root", type=Path, default=_repo_root(), help="Repo root (default: inferred from script path)")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing TOML files")

    sub = parser.add_subparsers(dest="cmd", required=True)

    p_char = sub.add_parser("character", help="Create a new assets/characters/<id>.toml")
    p_char.add_argument("--id", required=True, help="Character id (lowercase [a-z0-9_]+)")
    p_char.add_argument("--display", help="Display name (default: derived from id)")

    p_stage = sub.add_parser("stage", help="Create a new assets/stages/<id>.toml")
    p_stage.add_argument("--id", required=True, help="Stage id (lowercase [a-z0-9_]+)")
    p_stage.add_argument("--display", help="Display name (default: derived from id)")

    args = parser.parse_args(argv)
    opts = WriteOptions(repo_root=Path(cast(Path, args.repo_root)), overwrite=bool(args.overwrite))

    cmd = cast(str, args.cmd)
    if cmd == "character":
        return _cmd_character(args, opts)
    if cmd == "stage":
        return _cmd_stage(args, opts)
    raise RuntimeError(f"unknown command: {cmd}")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
