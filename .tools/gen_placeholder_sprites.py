#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.14"
# dependencies = []
# ///

from __future__ import annotations

import os
import struct
from collections.abc import Iterable
from dataclasses import dataclass


type RGB = tuple[int, int, int]


def clamp8(v: int) -> int:
    return max(0, min(255, int(v)))


def write_bmp(path: str, width: int, height: int, pixels: Iterable[RGB]) -> None:
    # 24-bit BMP, bottom-up rows, BGR order, row padded to 4 bytes.
    row_stride = width * 3
    pad = (4 - (row_stride % 4)) % 4
    data_size = (row_stride + pad) * height
    file_size = 14 + 40 + data_size

    with open(path, "wb") as f:
        # BITMAPFILEHEADER
        f.write(b"BM")
        f.write(struct.pack("<I", file_size))
        f.write(struct.pack("<HH", 0, 0))
        f.write(struct.pack("<I", 14 + 40))

        # BITMAPINFOHEADER
        f.write(struct.pack("<I", 40))  # header size
        f.write(struct.pack("<i", width))
        f.write(struct.pack("<i", height))
        f.write(struct.pack("<H", 1))  # planes
        f.write(struct.pack("<H", 24))  # bpp
        f.write(struct.pack("<I", 0))  # compression (BI_RGB)
        f.write(struct.pack("<I", data_size))
        f.write(struct.pack("<i", 2835))  # x ppm (72 DPI)
        f.write(struct.pack("<i", 2835))  # y ppm
        f.write(struct.pack("<I", 0))  # colors used
        f.write(struct.pack("<I", 0))  # important colors

        # Pixel data: pixels is top-down; BMP stores bottom-up.
        pix = list(pixels)
        assert len(pix) == width * height

        for y in range(height - 1, -1, -1):
            base = y * width
            for x in range(width):
                r, g, b = pix[base + x]
                f.write(bytes((clamp8(b), clamp8(g), clamp8(r))))
            if pad:
                f.write(b"\x00" * pad)


def set_px(buf: list[RGB], w: int, x: int, y: int, c: RGB) -> None:
    if x < 0 or y < 0 or x >= w:
        return
    buf[y * w + x] = c


def fill_rect(buf: list[RGB], w: int, x0: int, y0: int, x1: int, y1: int, c: RGB) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            set_px(buf, w, x, y, c)


def outline_rect(buf: list[RGB], w: int, x0: int, y0: int, x1: int, y1: int, c: RGB) -> None:
    for x in range(x0, x1):
        set_px(buf, w, x, y0, c)
        set_px(buf, w, x, y1 - 1, c)
    for y in range(y0, y1):
        set_px(buf, w, x0, y, c)
        set_px(buf, w, x1 - 1, y, c)


def draw_simple_actor(buf: list[RGB], sheet_w: int, fx: int, fy: int, color: RGB, variant: int) -> None:
    # Frame is 32x32, origin is top-left of frame.
    x0 = fx
    y0 = fy

    bg = (255, 0, 255)  # colorkey magenta
    outline = (12, 12, 12)
    accent = (min(255, color[0] + 40), min(255, color[1] + 40), min(255, color[2] + 40))

    fill_rect(buf, sheet_w, x0, y0, x0 + 32, y0 + 32, bg)

    # head + body
    fill_rect(buf, sheet_w, x0 + 12, y0 + 6, x0 + 20, y0 + 12, accent)
    fill_rect(buf, sheet_w, x0 + 11, y0 + 12, x0 + 21, y0 + 24, color)
    outline_rect(buf, sheet_w, x0 + 11, y0 + 6, x0 + 21, y0 + 24, outline)

    # legs (variant animates the pose)
    if variant == 0:
        fill_rect(buf, sheet_w, x0 + 12, y0 + 24, x0 + 15, y0 + 30, accent)
        fill_rect(buf, sheet_w, x0 + 17, y0 + 24, x0 + 20, y0 + 30, accent)
    elif variant == 1:
        fill_rect(buf, sheet_w, x0 + 11, y0 + 24, x0 + 14, y0 + 30, accent)
        fill_rect(buf, sheet_w, x0 + 18, y0 + 25, x0 + 21, y0 + 30, accent)
    elif variant == 2:
        fill_rect(buf, sheet_w, x0 + 12, y0 + 25, x0 + 15, y0 + 30, accent)
        fill_rect(buf, sheet_w, x0 + 17, y0 + 24, x0 + 20, y0 + 30, accent)
    else:
        fill_rect(buf, sheet_w, x0 + 11, y0 + 25, x0 + 14, y0 + 30, accent)
        fill_rect(buf, sheet_w, x0 + 18, y0 + 24, x0 + 21, y0 + 30, accent)

    # tiny eyes
    set_px(buf, sheet_w, x0 + 14, y0 + 8, outline)
    set_px(buf, sheet_w, x0 + 17, y0 + 8, outline)


def draw_dash_effect(buf: list[RGB], sheet_w: int, fx: int, fy: int, color: RGB) -> None:
    # trailing lines behind the actor
    dash = (min(255, color[0] + 50), min(255, color[1] + 50), min(255, color[2] + 50))
    for i in range(6):
        fill_rect(buf, sheet_w, fx + 2, fy + 14 + i, fx + 10, fy + 15 + i, dash)


def draw_glide_wings(buf: list[RGB], sheet_w: int, fx: int, fy: int, color: RGB) -> None:
    wing = (min(255, color[0] + 60), min(255, color[1] + 60), min(255, color[2] + 60))
    fill_rect(buf, sheet_w, fx + 6, fy + 14, fx + 11, fy + 18, wing)
    fill_rect(buf, sheet_w, fx + 21, fy + 14, fx + 26, fy + 18, wing)


def draw_spindash_ball(buf: list[RGB], sheet_w: int, fx: int, fy: int, color: RGB, phase: int) -> None:
    bg = (255, 0, 255)
    outline = (12, 12, 12)
    fill_rect(buf, sheet_w, fx, fy, fx + 32, fy + 32, bg)

    cx = fx + 16
    cy = fy + 18
    r = 9 + (phase % 2)
    for y in range(-r, r + 1):
        for x in range(-r, r + 1):
            if x * x + y * y <= r * r:
                set_px(buf, sheet_w, cx + x, cy + y, color)
    outline_rect(buf, sheet_w, cx - r, cy - r, cx + r + 1, cy + r + 1, outline)


@dataclass(frozen=True)
class CharacterSheetSpec:
    name: str
    color: RGB
    dash_effect: bool = True


def generate_sheet(path: str, spec: CharacterSheetSpec) -> None:
    frame_w = 32
    frame_h = 32
    cols = 4
    rows = 7  # idle, run, jump, fall, dash, glide, spindash_charge
    w = frame_w * cols
    h = frame_h * rows

    bg = (255, 0, 255)
    pixels: list[RGB] = [bg] * (w * h)

    # Row 0: idle (use frame 0)
    draw_simple_actor(pixels, w, 0 * frame_w, 0 * frame_h, spec.color, 0)
    for c in range(1, cols):
        draw_simple_actor(pixels, w, c * frame_w, 0 * frame_h, spec.color, 0)

    # Row 1: run (4 frames)
    for c in range(cols):
        draw_simple_actor(pixels, w, c * frame_w, 1 * frame_h, spec.color, c)

    # Row 2: jump
    draw_simple_actor(pixels, w, 0 * frame_w, 2 * frame_h, spec.color, 2)
    for c in range(1, cols):
        draw_simple_actor(pixels, w, c * frame_w, 2 * frame_h, spec.color, 2)

    # Row 3: fall
    draw_simple_actor(pixels, w, 0 * frame_w, 3 * frame_h, spec.color, 3)
    for c in range(1, cols):
        draw_simple_actor(pixels, w, c * frame_w, 3 * frame_h, spec.color, 3)

    # Row 4: dash
    for c in range(cols):
        draw_simple_actor(pixels, w, c * frame_w, 4 * frame_h, spec.color, 1)
        if spec.dash_effect:
            draw_dash_effect(pixels, w, c * frame_w, 4 * frame_h, spec.color)

    # Row 5: glide
    for c in range(cols):
        draw_simple_actor(pixels, w, c * frame_w, 5 * frame_h, spec.color, 0)
        draw_glide_wings(pixels, w, c * frame_w, 5 * frame_h, spec.color)

    # Row 6: spindash charge
    for c in range(cols):
        draw_spindash_ball(pixels, w, c * frame_w, 6 * frame_h, spec.color, c)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    write_bmp(path, w, h, pixels)


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "assets", "sprites")

    specs = [
        CharacterSheetSpec("sonic", (40, 140, 255)),
        CharacterSheetSpec("mario", (240, 70, 70)),
        CharacterSheetSpec("knuckles", (255, 90, 60)),
        CharacterSheetSpec("megamanx", (80, 200, 255), dash_effect=False),
    ]

    for s in specs:
        out_path = os.path.join(out_dir, f"{s.name}_placeholder.bmp")
        generate_sheet(out_path, s)
        print(f"Wrote {out_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
