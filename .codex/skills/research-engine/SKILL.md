---
name: research-engine
description: Study SDL3, ImGui, and engine architecture patterns.
---

# Research Engine

Study SDL3, ImGui, and engine architecture patterns relevant to this repo.

## Context to gather

Run these commands and include the output:

- `git log -n 15 --oneline`
- `git branch --show-current`
- `rg -n "imgui|SDL3|ImGui" src CMakeLists.txt 2>/dev/null || true`
- `just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Reference repositories

- `~/study/c++/imgui/` -- ImGui patterns, backend usage
- `~/study/c++/entt/` -- ECS patterns, performance implications

## Your task

1. Identify the area: SDL3 integration, ImGui UI, or ECS architecture
2. Find reference patterns in the study repos
3. Check AGENTS.md constraints (C++23, CMake + Ninja, warning hygiene)
4. Record findings and actionable recommendations

Output: Add findings to README or a notes file if present.
