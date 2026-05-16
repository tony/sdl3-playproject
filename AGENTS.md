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

## Shipped vs. Branch-Internal Narrative

Long-running branches accumulate tactical decisions — renames,
refactors, attempts-then-reverts, intermediate states. Commit messages
and the diff hold *what changed* and *why*. Do not restate either in
artifacts the downstream reader holds: code, docstrings, README,
CHANGES, PR descriptions, release notes, migration guides.

When deciding what counts as branch-internal, use trunk or the parent
branch as the baseline — not intermediate states inside the current
branch.

**The Published-Release Test**

Before adding rename history, "previously" / "formerly" / "no longer
X" phrasing, "removed" / "moved" / "refactored" / "fixed" diff
paraphrases, or `### Fixes` entries to a user-facing surface, ask:

> Did users of the most recently published release ever experience
> this old name, old behavior, or bug?

If the answer is no, it is branch-internal narrative. Move it to the
commit message and describe only the current state in the artifact.

**Keep in shipped artifacts**

- Deprecations and migration guides for symbols that actually shipped.
- `### Fixes` entries for bugs that affected users of a published
  release.
- Comments explaining *why the current code looks this way* —
  invariants, platform quirks, upstream bug workarounds — that make
  sense to a reader who never saw the previous version.

**Default**: when in doubt, keep the artifact clean and put the story
in the commit.

### Cleanup in Hindsight

When applying this rule retroactively from inside a feature branch,
first establish scope by diffing against the parent branch (or trunk)
to identify which commits this branch actually introduced. Then:

- **Commits introduced in this branch** — prompt the user with two
  options: `fixup!` commits with `git rebase --autosquash` to address
  each causal commit at its source, or a single cleanup commit at
  branch tip. User chooses.
- **Commits already in trunk or a parent branch** — default to
  leaving them alone. Do not raise them as cleanup candidates; act
  only on explicit user instruction. If the user opts in, fold the
  cleanup into a single commit at branch tip and do not rewrite trunk
  or parent-branch history.
- **Scope guard** — if cleaning in-branch bleed would touch a
  colleague's in-flight work or expand the branch beyond its stated
  goal, default to staying in lane: protect the project's current
  goal, leave prior bleed alone, and don't introduce new bleed in the
  current change.
