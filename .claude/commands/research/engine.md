---
description: Study SDL3, ImGui, and engine architecture patterns
allowed-tools: Bash(git *), Bash(just *), Read, Glob, Grep, Task
---

## Context (auto-gathered)

### Git state
- Recent commits: !`git log -n 15 --oneline`
- Current branch: !`git branch --show-current`

### Project references
- SDL/ImGui mentions: !`rg -n "imgui|SDL3|ImGui" src CMakeLists.txt 2>/dev/null | head -40 || true`
- Just commands: !`just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Reference repositories

- `~/study/c++/imgui/` — ImGui patterns, backend usage
- `~/study/c++/entt/` — ECS patterns, performance implications

## Your task

1. Identify the area: SDL3 integration, ImGui UI, or ECS architecture
2. Find reference patterns in the study repos
3. Check AGENTS.md constraints (C++23, CMake + Ninja, warning hygiene)
4. Record findings and actionable recommendations

**Output:** Record findings in README or a notes file if present.
