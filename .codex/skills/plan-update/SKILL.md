---
name: plan-update
description: Update planning notes and commit the changes.
---

# Plan Update

Update planning notes and commit documentation changes.

## Context to gather

Run these commands and include the output:

- `git branch --show-current`
- `git status --short`
- `git log -n 10 --oneline`
- `ls -la notes 2>/dev/null || echo "No notes directory"`
- `ls -la UNREAL_CONVERSION_PLAN.md 2>/dev/null || echo "No UNREAL_CONVERSION_PLAN.md"`

## Your task

1. If `notes/plan.md` exists, read it fully and update status markers
2. If there is no notes plan, update `UNREAL_CONVERSION_PLAN.md` with current progress
3. Cross-reference with recent commits to mark completed items
4. Add new items discovered during recent work
5. Commit independently:

```
docs(notes) update plan status for <topic>

why: Keep plan in sync with actual progress
what:
- Mark <items> as completed
- Add <new items> discovered
```

Important:
- Do NOT start implementation
- Only update notes/plan docs
- Keep the commit focused on documentation changes only
