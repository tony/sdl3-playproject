#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# dependencies = ["tomlkit"]
# ///
"""
Migrate character TOML files from px/s units to px/frame units at 120Hz.

Conversion formulas:
- velocity: px_frame = px_s / 120
- acceleration: px_frame2 = px_s2 / 14400
- timer: frames = ms * 120 / 1000

Also renames *_ms fields to *_frames.
"""

from pathlib import Path
import tomlkit

# Velocity fields (divide by 120)
VELOCITY_FIELDS = [
    "max_speed_ground",
    "max_speed_air",
    "accel_ground",
    "decel_ground",
    "skid_decel",
    "air_accel",
    "air_decel",
    "impulse",  # jump impulse
    "release_decay",
    "min_hold_velocity",
    "speed",
    "launch_speed",  # dash/spindash
    "max_charge_speed",
    "speed_per_charge",
    "glide_speed",
    "glide_gravity",
    "glide_decel",
    "glide_fall_speed",
    "bounce_speed",
    "fly_up_speed",
    "fly_accel",
    "fly_decel",
    "air_dash_speed",
    "wall_slide_speed",
    "wall_climb_speed",
    "wall_jump_impulse_x",
    "wall_jump_impulse_y",
    "wall_kick_impulse_x",
    "wall_kick_impulse_y",
    "slide_speed",
    "step_height",  # This is actually pixels, not velocity - skip
]

# Acceleration fields (divide by 14400)
ACCEL_FIELDS = [
    "gravity",
    "gravity_up",
    "gravity_down",
]

# Timer fields (multiply by 120 / 1000, rename _ms to _frames)
TIMER_FIELDS_MS = [
    "coyote_time_ms",
    "jump_buffer_ms",
    "cooldown_ms",
    "time_ms",
    "duration_ms",
    "max_charge_ms",
    "charge_interval_ms",
    "fly_stamina_ms",
    "fly_recharge_rate_ms",
]


def convert_velocity(value: float) -> float:
    """Convert px/s to px/frame at 120Hz."""
    return round(value / 120.0, 6)


def convert_accel(value: float) -> float:
    """Convert px/s² to px/frame² at 120Hz."""
    return round(value / 14400.0, 6)


def convert_timer_ms_to_frames(value: int | float) -> int:
    """Convert ms to frames at 120Hz."""
    return round(value * 120 / 1000)


def is_velocity_field(key: str) -> bool:
    """Check if field should be converted as velocity."""
    # Skip step_height as it's in pixels
    if key == "step_height":
        return False
    return key in VELOCITY_FIELDS


def is_accel_field(key: str) -> bool:
    """Check if field should be converted as acceleration."""
    return key in ACCEL_FIELDS


def is_timer_ms_field(key: str) -> bool:
    """Check if field is a millisecond timer."""
    return key in TIMER_FIELDS_MS


def migrate_table(table: tomlkit.items.Table, parent_key: str = "") -> list[str]:
    """Recursively migrate a TOML table. Returns list of changes made."""
    changes: list[str] = []
    keys_to_rename: list[tuple[str, str, object]] = []

    for key, value in table.items():
        full_key = f"{parent_key}.{key}" if parent_key else key

        if isinstance(value, tomlkit.items.Table):
            changes.extend(migrate_table(value, full_key))
        elif isinstance(value, (int, float)):
            if is_velocity_field(key):
                old_val = value
                new_val = convert_velocity(float(value))
                table[key] = new_val
                changes.append(f"{full_key}: {old_val} -> {new_val} (px/frame)")
            elif is_accel_field(key):
                old_val = value
                new_val = convert_accel(float(value))
                table[key] = new_val
                changes.append(f"{full_key}: {old_val} -> {new_val} (px/frame²)")
            elif is_timer_ms_field(key):
                old_val = value
                new_val = convert_timer_ms_to_frames(value)
                new_key = key.replace("_ms", "_frames")
                keys_to_rename.append((key, new_key, new_val))
                changes.append(f"{full_key}: {old_val}ms -> {new_val} frames")

    # Apply key renames after iteration
    for old_key, new_key, new_val in keys_to_rename:
        del table[old_key]
        table[new_key] = new_val

    return changes


def migrate_file(path: Path) -> list[str]:
    """Migrate a single TOML file. Returns list of changes made."""
    content = path.read_text()
    doc = tomlkit.parse(content)
    changes = migrate_table(doc)

    if changes:
        path.write_text(tomlkit.dumps(doc))

    return changes


def main() -> None:
    """Migrate all character TOML files."""
    assets_dir = Path(__file__).parent.parent / "assets" / "characters"
    toml_files = sorted(assets_dir.glob("*.toml"))

    print(f"Migrating {len(toml_files)} character files to 120Hz frame-based units...")
    print()

    total_changes = 0
    for path in toml_files:
        changes = migrate_file(path)
        if changes:
            print(f"{path.name}:")
            for change in changes:
                print(f"  {change}")
            total_changes += len(changes)
            print()

    print(f"Done! Made {total_changes} changes across {len(toml_files)} files.")


if __name__ == "__main__":
    main()
