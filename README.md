# SDL3 sandbox (bare)

A minimal SDL3 C++23 app meant as a starting point for any game type. The full
platformer sandbox (characters, stages, assets, and tools) lives on the
`platformer` branch.

## Ubuntu 24.04 dependencies

```bash
sudo apt update
```

```bash
sudo apt install -y build-essential cmake ninja-build just git pkg-config \
  clang-format clang-tidy cppcheck cpplint iwyu
```

## Optional: SDL3 system feature deps (Linux)

SDL3 is built from source via FetchContent and will auto-detect optional system
libraries. Installing these enables more audio/video/input backends on Linux.

```bash
sudo apt install -y \
  libasound2-dev libpulse-dev libaudio-dev libjack-dev \
  libsndio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev libwayland-dev \
  libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev \
  libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libibus-1.0-dev libudev-dev fcitx-libs-dev
```

## Build + run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

```bash
cmake --build build
```

```bash
./build/sandbox
```

### Controls

- Arrow keys: move the square
- Esc: quit

### Useful flags

- `--frames N` (smoke test and exit after N frames)
- `--video-driver offscreen` (headless window for CI)
- `--sprite PATH` (optional PNG/BMP to render)
- `--width W` / `--height H`

## Tests

```bash
ctest --test-dir build --output-on-failure
```
