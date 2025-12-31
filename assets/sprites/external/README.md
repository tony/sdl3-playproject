# External sprites (local-only)

This folder is for **downloaded sprite sheets** that are not committed to the repo.

The sandbox ships with `*_placeholder.bmp` so we can iterate without pulling in
copyrighted game assets. If you download third-party sprite sheets for personal
testing, place them here.

This directory is `.gitignore`'d (except for this README).

## Download helper

Run:

```bash
uv run tools/fetch_spriters_resource_sheets.py
```

This writes into `assets/sprites/external/`.
