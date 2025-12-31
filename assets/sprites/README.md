# Sprites (placeholders)

This folder contains **original placeholder** sprite sheets for the sandbox so we can exercise:

- sprite-sheet generation + validation (and optional sprite renderer experiments)
- basic animation state selection (idle/run/jump/fall/dash/glide/spindash charge)
- per-character visual identity without pulling in copyrighted game assets

The files named `*_placeholder.bmp` are intentionally simple and meant to be replaced later with your own art.

Note: the current default renderer uses **procedural, shape-based character forms**. These BMPs are kept around as placeholders and for future sprite-based rendering experiments.

## Transparency

The placeholder BMPs use **magenta** (`#FF00FF`) as a transparency key. The runtime sets this as a color key when loading.

## Regenerating

Run:

```bash
uv run tools/gen_placeholder_sprites.py
```
