#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# dependencies = ["pillow"]
# ///
"""
Combine individual PNG sprite frames into sprite sheets.

Creates one sprite sheet per character per direction:
  {char}_{direction}.png (e.g., bolt_east.png)

Sprite sheet layout (rows):
  Row 0: idle (breathing-idle)
  Row 1: run (running-6-frames)
  Row 2: jump (two-footed-jump)
  Row 3: special (hurricane-kick / running-slide / jumping-1)
  Row 4: attack (cross-punch)

Each row has max_frames columns (determined by longest animation).
"""

from pathlib import Path
from PIL import Image
import json


# Animation name mapping: game state -> pixellab directory name
ANIM_ORDER: list[tuple[str, str]] = [
    ("idle", "breathing-idle"),
    ("run", "running-6-frames"),
    ("jump", "two-footed-jump"),
    ("special", None),  # Varies per character
    ("attack", "cross-punch"),
]

# Character-specific special animation
SPECIAL_ANIMS: dict[str, str] = {
    "bolt": "hurricane-kick",
    "gale": "hurricane-kick",
    "vex": "running-slide",
    "forge": "running-slide",
    "nimbus": "jumping-1",
}

# Enemy animation mappings (different from heroes)
ENEMY_ANIM_ORDER: dict[str, list[tuple[str, str]]] = {
    "slime": [
        ("idle", "breathing-idle"),
        ("walk", "walking"),
        ("jump", "jumping-1"),
        ("hurt", "taking-punch"),
    ],
    "bat": [
        ("idle", "breathing-idle"),
        ("walk", "walking-6-frames"),
        ("fly", "jumping-1"),
        ("hurt", "taking-punch"),
    ],
    "skeleton": [
        ("idle", "breathing-idle"),
        ("walk", "walking-6-frames"),
        ("attack", "cross-punch"),
        ("hurt", "falling-back-death"),
    ],
}

DIRECTIONS: list[str] = ["south", "west", "east", "north"]
FRAME_SIZE: int = 64  # Pixels


def get_frame_files(anim_dir: Path, direction: str) -> list[Path]:
    """Get sorted frame files for an animation direction."""
    dir_path = anim_dir / direction
    if not dir_path.exists():
        return []
    frames = sorted(dir_path.glob("frame_*.png"))
    return frames


def count_max_frames(char_dir: Path, char_name: str) -> int:
    """Determine the maximum frame count across all animations."""
    max_frames = 0
    anims_dir = char_dir / "animations"

    for state_name, pixellab_name in ANIM_ORDER:
        if pixellab_name is None:
            pixellab_name = SPECIAL_ANIMS.get(char_name, "")
        if not pixellab_name:
            continue

        anim_path = anims_dir / pixellab_name
        if anim_path.exists():
            # Check first direction that exists
            for d in DIRECTIONS:
                frames = get_frame_files(anim_path, d)
                if frames:
                    max_frames = max(max_frames, len(frames))
                    break

    return max_frames


def create_sprite_sheet(
    char_dir: Path, char_name: str, direction: str, max_frames: int
) -> tuple[Image.Image, dict[str, dict[str, int]]]:
    """Create sprite sheet for one character and direction."""
    num_rows = len(ANIM_ORDER)
    sheet_width = max_frames * FRAME_SIZE
    sheet_height = num_rows * FRAME_SIZE

    # Create RGBA sheet with transparent background
    sheet = Image.new("RGBA", (sheet_width, sheet_height), (0, 0, 0, 0))
    anims_dir = char_dir / "animations"

    # Metadata for TOML config generation
    anim_metadata: dict[str, dict[str, int]] = {}

    for row, (state_name, pixellab_name) in enumerate(ANIM_ORDER):
        if pixellab_name is None:
            pixellab_name = SPECIAL_ANIMS.get(char_name, "")
        if not pixellab_name:
            continue

        anim_path = anims_dir / pixellab_name
        frames = get_frame_files(anim_path, direction)

        if not frames:
            print(f"  Warning: No frames for {char_name}/{pixellab_name}/{direction}")
            continue

        # Paste frames into sheet
        for col, frame_path in enumerate(frames):
            frame = Image.open(frame_path)
            x = col * FRAME_SIZE
            y = row * FRAME_SIZE
            sheet.paste(frame, (x, y))

        # Store metadata
        anim_metadata[state_name] = {
            "row": row,
            "frames": len(frames),
            "source": pixellab_name,
        }

    return sheet, anim_metadata


def process_character(char_dir: Path, output_dir: Path) -> dict[str, object]:
    """Process all directions for one character."""
    char_name = char_dir.name
    print(f"Processing {char_name}...")

    max_frames = count_max_frames(char_dir, char_name)
    print(f"  Max frames per row: {max_frames}")

    all_metadata: dict[str, object] = {
        "character": char_name,
        "frame_size": FRAME_SIZE,
        "max_frames": max_frames,
        "directions": {},
    }

    for direction in DIRECTIONS:
        print(f"  Creating {direction} sheet...")
        sheet, anim_meta = create_sprite_sheet(char_dir, char_name, direction, max_frames)

        # Save PNG
        out_path = output_dir / f"{char_name}_{direction}.png"
        sheet.save(out_path, "PNG")
        print(f"    Saved: {out_path}")

        all_metadata["directions"][direction] = {
            "sheet": str(out_path.relative_to(output_dir.parent.parent)),
            "animations": anim_meta,
        }

    return all_metadata


def count_enemy_max_frames(enemy_dir: Path, enemy_name: str) -> int:
    """Determine the maximum frame count for an enemy's animations."""
    max_frames = 0
    anims_dir = enemy_dir / "animations"
    anim_order = ENEMY_ANIM_ORDER.get(enemy_name, [])

    for _state_name, pixellab_name in anim_order:
        if not pixellab_name:
            continue
        anim_path = anims_dir / pixellab_name
        if anim_path.exists():
            for d in DIRECTIONS:
                frames = get_frame_files(anim_path, d)
                if frames:
                    max_frames = max(max_frames, len(frames))
                    break

    return max_frames


def create_enemy_sprite_sheet(
    enemy_dir: Path, enemy_name: str, direction: str, max_frames: int
) -> tuple[Image.Image, dict[str, dict[str, int]]]:
    """Create sprite sheet for one enemy and direction."""
    anim_order = ENEMY_ANIM_ORDER.get(enemy_name, [])
    num_rows = len(anim_order)
    sheet_width = max_frames * FRAME_SIZE
    sheet_height = num_rows * FRAME_SIZE

    sheet = Image.new("RGBA", (sheet_width, sheet_height), (0, 0, 0, 0))
    anims_dir = enemy_dir / "animations"
    anim_metadata: dict[str, dict[str, int]] = {}

    for row, (state_name, pixellab_name) in enumerate(anim_order):
        if not pixellab_name:
            continue

        anim_path = anims_dir / pixellab_name
        frames = get_frame_files(anim_path, direction)

        if not frames:
            print(f"  Warning: No frames for {enemy_name}/{pixellab_name}/{direction}")
            continue

        for col, frame_path in enumerate(frames):
            frame = Image.open(frame_path)
            x = col * FRAME_SIZE
            y = row * FRAME_SIZE
            sheet.paste(frame, (x, y))

        anim_metadata[state_name] = {
            "row": row,
            "frames": len(frames),
            "source": pixellab_name,
        }

    return sheet, anim_metadata


def process_enemy(enemy_dir: Path, output_dir: Path) -> dict[str, object]:
    """Process all directions for one enemy."""
    enemy_name = enemy_dir.name
    print(f"Processing enemy: {enemy_name}...")

    max_frames = count_enemy_max_frames(enemy_dir, enemy_name)
    print(f"  Max frames per row: {max_frames}")

    all_metadata: dict[str, object] = {
        "enemy": enemy_name,
        "frame_size": FRAME_SIZE,
        "max_frames": max_frames,
        "directions": {},
    }

    for direction in DIRECTIONS:
        print(f"  Creating {direction} sheet...")
        sheet, anim_meta = create_enemy_sprite_sheet(
            enemy_dir, enemy_name, direction, max_frames
        )

        out_path = output_dir / f"{enemy_name}_{direction}.png"
        sheet.save(out_path, "PNG")
        print(f"    Saved: {out_path}")

        all_metadata["directions"][direction] = {
            "sheet": str(out_path.relative_to(output_dir.parent.parent)),
            "animations": anim_meta,
        }

    return all_metadata


def main() -> None:
    """Process all characters and enemies, generate sprite sheets."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    pixellab_dir = project_root / "assets" / "sprites" / "pixellab"

    if not pixellab_dir.exists():
        print(f"Error: {pixellab_dir} not found")
        return

    # Output directory for combined sheets
    output_dir = pixellab_dir / "sheets"
    output_dir.mkdir(exist_ok=True)

    all_metadata: dict[str, object] = {}

    # Process each hero character (top-level dirs with animations/)
    skip_dirs = {"sheets", "enemies", "objects", "tilesets"}
    for char_dir in sorted(pixellab_dir.iterdir()):
        if char_dir.is_dir() and char_dir.name not in skip_dirs:
            anims_dir = char_dir / "animations"
            if anims_dir.exists():
                all_metadata[char_dir.name] = process_character(char_dir, output_dir)

    # Process enemies from enemies/ subdirectory
    enemies_dir = pixellab_dir / "enemies"
    if enemies_dir.exists():
        print("\n--- Processing Enemies ---")
        for enemy_dir in sorted(enemies_dir.iterdir()):
            if enemy_dir.is_dir() and enemy_dir.name in ENEMY_ANIM_ORDER:
                all_metadata[enemy_dir.name] = process_enemy(enemy_dir, output_dir)

    # Save combined metadata
    meta_path = output_dir / "metadata.json"
    with open(meta_path, "w") as f:
        json.dump(all_metadata, f, indent=2)
    print(f"\nMetadata saved to: {meta_path}")

    # Print TOML config snippet for reference
    print("\n" + "=" * 60)
    print("Example TOML config for render_bolt_pixellab.toml:")
    print("=" * 60)
    if "bolt" in all_metadata:
        bolt = all_metadata["bolt"]
        east_anims = bolt["directions"]["east"]["animations"]  # type: ignore
        print(f"""
[render]
sheet = "assets/sprites/pixellab/sheets/bolt_east.png"
frame_w = {FRAME_SIZE}
frame_h = {FRAME_SIZE}
scale = 2.0

[render.anims.idle]
row = {east_anims.get("idle", {}).get("row", 0)}
frames = {east_anims.get("idle", {}).get("frames", 1)}
fps = 8.0

[render.anims.run]
row = {east_anims.get("run", {}).get("row", 1)}
frames = {east_anims.get("run", {}).get("frames", 1)}
fps = 12.0

[render.anims.jump]
row = {east_anims.get("jump", {}).get("row", 2)}
frames = {east_anims.get("jump", {}).get("frames", 1)}
fps = 12.0

[render.anims.spin]
row = {east_anims.get("special", {}).get("row", 3)}
frames = {east_anims.get("special", {}).get("frames", 1)}
fps = 16.0

[render.anims.attack_melee]
row = {east_anims.get("attack", {}).get("row", 4)}
frames = {east_anims.get("attack", {}).get("frames", 1)}
fps = 12.0
""")


if __name__ == "__main__":
    main()
