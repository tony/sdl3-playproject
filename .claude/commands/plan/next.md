---
description: Analyze repo state and suggest pragmatic next steps
allowed-tools: Bash(git *), Bash(just *), Read, Glob, Grep
---

## Context (auto-gathered)

### Git state
- Branch: !`git branch --show-current`
- Status: !`git status --short`
- Recent commits: !`git log -n 15 --oneline`
- Diff vs origin/master: !`git diff origin/master | head -200`

### Project state
- Plan doc: !`ls -la UNREAL_CONVERSION_PLAN.md 2>/dev/null || echo "No UNREAL_CONVERSION_PLAN.md"`
- TODOs: !`rg -n "TODO|FIXME" src assets 2>/dev/null | head -40 || true`

### Available tools
- Just commands: !`just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Your task

Based on AGENTS.md, README.md, and UNREAL_CONVERSION_PLAN.md (if present):

1. **Identify completed items** — check against recent commits
2. **Find leftovers** — incomplete work from recent changes or asset integration
3. **List low-hanging fruit** — quick wins, unblocked items
4. **Note blockers** — dependencies, assets, or design decisions needed

**Prioritize by:**
- Unblocks other work (highest)
- Small effort + clear scope
- High impact on stability/gameplay

**Output format:**
```
## Recommended Next Steps

1. [HIGH] <task> — <why it's priority>
2. [MED] <task> — <context>
3. [LOW] <task> — <when to tackle>

## Blockers / Decisions Needed
- <item needing clarification>
```

Do NOT start implementation — analyze and recommend only.
