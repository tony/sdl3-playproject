---
name: plan-next
description: Analyze repo state and suggest pragmatic next steps.
---

# Plan Next

Analyze current state and propose next steps aligned with AGENTS.md.

## Context to gather

Run these commands and include the output:

- `git branch --show-current`
- `git status --short`
- `git log -n 15 --oneline`
- `git diff origin/master`
- `rg -n "TODO|FIXME" src assets 2>/dev/null || true`
- `just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`
- `ls -la UNREAL_CONVERSION_PLAN.md 2>/dev/null || echo "No UNREAL_CONVERSION_PLAN.md"`

## Your task

Based on AGENTS.md, README.md, and UNREAL_CONVERSION_PLAN.md (if present):

1. Identify completed items from recent commits
2. Find leftovers from recent refactors or asset work
3. List low-hanging fruit with clear scope
4. Note blockers or missing assets

Prioritize by:
- Unblocks other work (highest)
- Small effort + clear scope
- High impact on stability or core gameplay

Output format:

```
## Recommended Next Steps

1. [HIGH] <task> -- <why it's priority>
2. [MED] <task> -- <context>
3. [LOW] <task> -- <when to tackle>

## Blockers / Decisions Needed
- <item needing clarification>
```

Do NOT start implementation -- analyze and recommend only.
