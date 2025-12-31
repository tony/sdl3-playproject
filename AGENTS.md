# AGENTS.md

This file captures the rules and conventions for both humans and AI agents
working in this repository.

## Project Philosophy

- This repository is a sandbox, not a general-purpose engine.
- Prefer explicit, boring C++ over clever abstractions.
- SDL3 is treated as a platform API, not a framework.
- Keep systems small and purpose-built; avoid meta-architectures.

## Language and Tooling

- C++23 only.
- CMake + Ninja only.
- No compiler-specific extensions unless gated and documented.
- Code must compile cleanly on both Clang and GCC.
- Keep warnings clean at reasonable levels (`-Wall -Wextra -Wpedantic`).

## Ubuntu setup

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build just git pkg-config clang-format clang-tidy cppcheck cpplint iwyu
```

Optional (Linux): SDL3 system feature deps (enables more backends when SDL is built from source):

```bash
sudo apt install -y \
  libasound2-dev libpulse-dev libaudio-dev libjack-dev \
  libsndio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev libwayland-dev \
  libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev \
  libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libibus-1.0-dev libudev-dev fcitx-libs-dev
```

## Scripts (Python)

- Prefer `uv` / `uvx` for running any Python tooling (no checked-in venvs).
- Python scripts should be runnable with `uv run` and use PEP 723 inline metadata:
  - Shebang: `#!/usr/bin/env -S uv run --script`
  - `requires-python = ">=3.14"`
  - Explicit `dependencies = [...]` (empty is OK).
- Scripts must be fully type-annotated and intended to pass `uvx mypy --strict`.

## Directory Ownership

- `core/`: Application lifecycle, timing, input, rendering, and top-level orchestration.
- New subsystems should live in their own top-level directory (e.g., `audio/`, `ui/`).

## Input Handling

- SDL input is normalized in one place.
- Gameplay/logic consumes normalized input data and never polls SDL directly.
- Input timing (pressed, held, released) should be explicit.

## Rendering

- Rendering is separate from simulation.
- Render code must never mutate gameplay state.
- Debug rendering is allowed but must be clearly scoped and optional.

## AI / Agent Contributions

- AI-generated code is held to the same standard as human-written code.
- No bulk rewrites without explicit intent.
- Generated code must be understandable without access to the prompt.

## Git Commit Standards

Format commit messages as:
```
commit-type(Component/File[Subcomponent/method]) Concise description

why: Explanation of necessity or impact.
what:
- Specific technical changes made
- Focused on a single topic
```

Commit titles start with a **lowercase** commit type. Use uppercase only for
properly-capitalized identifiers (e.g. class names) inside the scope.

Common commit types:
- **feat**: New features or enhancements
- **fix**: Bug fixes
- **refactor**: Code restructuring without functional change
- **docs**: Documentation updates
- **chore**: Maintenance (dependencies, tooling, config)
- **test**: Test-related updates
- **style**: Code style and formatting
- **js(deps)**: Dependencies
- **js(deps[dev])**: Dev Dependencies
- **ai(rules[LLM type])**: AI Rule Updates
