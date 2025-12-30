# AGENTS.md

This file captures the rules and conventions for both humans and AI agents working in this repository.

## Project Philosophy

- This repository is a sandbox, not a general-purpose engine.
- Prefer explicit, boring C++ over clever abstractions.
- Data-driven behavior beats inheritance-heavy designs.
- SDL3 is treated as a platform API, not a framework.
- ECS is minimal and purpose-built; avoid meta-architectures.

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

- `core/`: Application lifecycle, timing, and top-level orchestration. No gameplay or character logic.
- `ecs/`: Entities, components, and systems. Components are passive data; systems own behavior.
- `stage/`: Level geometry, collision surfaces, and spawn logic. No character-specific rules.
- `character/`: Character movement, abilities, and configuration. Must remain data-driven via TOML.
- `assets/`: Runtime data only (TOML, art placeholders, etc.). No behavior assumptions encoded in code.

## Character Framework Rules

- Character behavior must be configurable via TOML.
- Adding a new character must not require new subclasses.
- Differences between characters are expressed via:
  - Numeric parameters
  - Feature flags
  - Action configuration
- Actions (jump, dash, spin, fly, etc.):
  - Are optional
  - Are parameterized
  - Are evaluated from `InputState`, not hard-coded key checks
- The controller remains generic and deterministic.

## ECS Rules of the Road

- Entities are lightweight IDs.
- Components contain no logic.
- Systems:
  - Operate over explicit component sets
  - Are deterministic given input + config
  - Do not silently create or destroy entities
- **Exception:** EnTT is allowed as the ECS foundation—it provides the registry,
  views, and entity management. Custom "meta-architectures" built on top are still discouraged.
- EnTT's compile-time features are acceptable; avoid additional template metaprogramming layers.
- Favor clarity, debuggability, and predictable data flow.

## Input Handling

- SDL input is normalized into `InputState` components.
- SDL key mappings live in one place only.
- Gameplay systems consume input data; they never poll SDL directly.
- Input timing (pressed, held, released) is explicit.

## Physics and Collision

- Collision resolution favors stability and feel over realism.
- Platformer concepts (grounded state, coyote time, jump buffering) are first-class.
- No magic numbers without a named constant or config field.
- Any non-obvious physics tweak requires a comment explaining intent.

## Rendering

- Rendering is strictly separate from simulation.
- Render code must never mutate gameplay state.
- Debug rendering is allowed but must be clearly scoped and optional.
- SDL rendering APIs should not leak into gameplay systems.

## Configuration (TOML)

- TOML files are part of the public API of the sandbox.
- Backwards compatibility matters once a field ships.
- Prefer additive changes over semantic changes.
- Defaults must be defined in code and match expected behavior.

## AI / Agent Contributions

- AI-generated code is held to the same standard as human-written code.
- No bulk rewrites without explicit intent.
- Any AI-related rule changes use `ai(rules[...])` commit types.
- Generated code must be understandable without access to the prompt.

## What This Repo Is Not

- Not a full-featured game engine.
- Not a physics research project.
- Not a rendering showcase.
- Not optimized for speculative extensibility.

## Git Commit Standards

Format commit messages as:
```
commit-type(Component/File[Subcomponent/method]) Concise description

why: Explanation of necessity or impact.
what:
- Specific technical changes made
- Focused on a single topic
```

Commit titles start with a **lowercase** commit type. Use uppercase only for properly-capitalized identifiers (e.g. class names) inside the scope.

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

Example:
```
feat(store[search/mobx]) Add bidirectional sync for filter directives

why: Keep search input and filter UI in sync when toggling de minimis
what:
- Add directive sync to toggleDeMinimis action
- Update search input when filter changes programmatically
- Add tests for bidirectional sync behavior
```
