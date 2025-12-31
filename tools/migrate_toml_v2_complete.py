#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Complete migration of character TOML files from px/s to px/frame units.

At 120Hz:
- Velocity: px_frame = px_s / 120
- Acceleration: px_frame2 = px_s2 / 14400
- Timer: frames = ms / (1000/120) = ms * 0.12
"""

import re
from pathlib import Path

# Conversion factors at 120Hz
VEL_DIVISOR = 120.0
ACCEL_DIVISOR = 14400.0  # 120^2
MS_TO_FRAMES = 120.0 / 1000.0

# Fields that are velocities (divide by 120)
VELOCITY_FIELDS = {
    'max_speed_ground',
    'max_speed_air',
    'max_fall_speed',
    'impulse',          # jump impulse
    'dash_speed',
    'dash_kick_speed',
    'min_launch_speed',
    'max_launch_speed',
    'slide_speed',
    'climb_speed',
    'descend_speed',
    'jump_impulse',
    'jump_vx',
    'knockback_x',
    'knockback_y',
    'stop_speed',
}

# Fields that are accelerations (divide by 14400)
ACCEL_FIELDS = {
    'gravity',
    'accel_ground',
    'accel_air',
    'decel_ground',
    'decel_air',
}

# Timer fields: rename from _ms to _frames and convert value
TIMER_RENAMES = {
    'coyote_time_ms': 'coyote_frames',
    'jump_buffer_ms': 'jump_buffer_frames',
    'cooldown_ms': 'cooldown_frames',
    'dash_time_ms': 'dash_time_frames',
    'charge_time_ms': 'charge_frames',
    'tap_boost_ms': 'tap_boost_frames',
    'detach_ms': 'detach_frames',
    'jump_lockout_ms': 'jump_lockout_frames',
}

def convert_value(value: float, divisor: float) -> str:
    """Convert a value and format it nicely."""
    result = value / divisor
    # Format with appropriate precision
    if abs(result - round(result)) < 0.0001:
        return str(int(round(result)))
    elif result >= 1.0:
        return f"{result:.2f}"
    else:
        return f"{result:.6f}".rstrip('0').rstrip('.')

def ms_to_frames(value_ms: float) -> int:
    """Convert milliseconds to frames at 120Hz."""
    return round(value_ms * MS_TO_FRAMES)

def process_file(path: Path) -> tuple[bool, list[str]]:
    """Process a single TOML file. Returns (modified, changes)."""
    content = path.read_text()
    lines = content.split('\n')
    new_lines = []
    changes = []
    modified = False

    for line in lines:
        new_line = line

        # Match field = value (with optional comment)
        m = re.match(r'^(\s*)(\w+)(\s*=\s*)(-?[\d.]+)(\s*(?:#.*)?)$', line)
        if m:
            indent = m.group(1)
            field = m.group(2)
            equals = m.group(3)
            value_str = m.group(4)
            comment = m.group(5)
            value = float(value_str)

            # Check if it's a velocity field
            if field in VELOCITY_FIELDS:
                new_value = convert_value(value, VEL_DIVISOR)
                new_line = f"{indent}{field}{equals}{new_value}{comment}"
                if new_line != line:
                    changes.append(f"  {field}: {value} px/s -> {new_value} px/frame")
                    modified = True

            # Check if it's an acceleration field
            elif field in ACCEL_FIELDS:
                new_value = convert_value(value, ACCEL_DIVISOR)
                new_line = f"{indent}{field}{equals}{new_value}{comment}"
                if new_line != line:
                    changes.append(f"  {field}: {value} px/s² -> {new_value} px/frame²")
                    modified = True

            # Check if it's a timer field to rename
            elif field in TIMER_RENAMES:
                new_field = TIMER_RENAMES[field]
                new_value = ms_to_frames(value)
                new_line = f"{indent}{new_field}{equals}{new_value}{comment}"
                changes.append(f"  {field}: {value}ms -> {new_field}: {new_value} frames")
                modified = True

        new_lines.append(new_line)

    if modified:
        path.write_text('\n'.join(new_lines))

    return modified, changes

def main():
    characters_dir = Path('assets/characters')
    if not characters_dir.exists():
        characters_dir = Path('/home/d/work/c++/sdl3-playproject/assets/characters')

    total_changes = 0
    for toml_path in sorted(characters_dir.glob('*.toml')):
        modified, changes = process_file(toml_path)
        if modified:
            print(f"\n{toml_path.name}:")
            for change in changes:
                print(change)
            total_changes += len(changes)

    print(f"\n\nTotal: {total_changes} fields converted")

if __name__ == '__main__':
    main()
