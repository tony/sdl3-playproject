# Input scripts (deterministic playback)

Input scripts are small TOML files that drive gameplay input deterministically for smoke tests and
repro runs (via `--input-script PATH`).

## Format

```toml
version = 1
include = "move_instant_run_right.toml"

[[keyframes]]
frame = 0
right = true

[[keyframes]]
frame = 45
right = false
```

- `version`: currently `1`.
- `include`: optional relative path to another script to load first.
- `[[keyframes]]`: list of keyframes. (Legacy alias: `[[frames]]`.)
- `frame` (or legacy `at`): integer simulation frame index (0-based).
- Input booleans (optional): `left`, `right`, `up`, `down`, `jump`, `action1`, `action2`.

## Semantics

- Keyframes are applied in ascending `frame` order (ordering in the file does not matter).
- Inputs are **held** until changed by a later keyframe.
- `pressed` / `released` edges are derived deterministically from held state transitions.
- `include` is resolved relative to the script file and is processed before the current file.

## Useful CLI patterns

- Smoke run (offscreen):
  - `./build/sandbox --no-prefs --frames 120 --video-driver offscreen --stage assets/dev/stages/slope_torture.toml --spawn-point subpixel_seam --input-script assets/input_scripts/slope_torture_subpixel_traverse.toml`
- Assertions for CI-style traversal checks:
  - `--expect-grounded --expect-no-respawn --expect-min-x 2800 --expect-max-y 640`
  - Floor / drop-through checks:
    - `--expect-grounded --expect-no-respawn --expect-min-y 440`
  - Camera checks:
    - `--expect-no-camera-backscroll`
  - Velocity checks (useful for actions like dash/spindash):
    - `--expect-min-vx 900`
  - Left-moving or “x bracket” checks:
    - `--expect-min-x 320 --expect-max-x 370 --expect-max-y 640`
