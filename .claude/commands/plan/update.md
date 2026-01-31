---
description: Update planning notes and commit the changes
allowed-tools: Bash(git add:*), Bash(git status:*), Bash(git commit:*), Bash(git diff:*), Read, Edit
---

## Context (auto-gathered)

### Git state
- Branch: !`git branch --show-current`
- Status: !`git status --short`
- Recent commits: !`git log -n 10 --oneline`

### Project state
- Notes dir: !`ls -la notes 2>/dev/null || echo "No notes directory"`
- UNREAL plan: !`ls -la UNREAL_CONVERSION_PLAN.md 2>/dev/null || echo "No UNREAL_CONVERSION_PLAN.md"`

## Your task

1. **If `notes/plan.md` exists**, read it fully and update status markers
2. **If no notes plan**, update `UNREAL_CONVERSION_PLAN.md` with current progress
3. **Cross-reference** with recent commits to find completed items
4. **Add new items** discovered during recent work
5. **Commit independently:**

```
docs(notes) update plan status for <topic>

why: Keep plan in sync with actual progress
what:
- Mark <items> as completed
- Add <new items> discovered
```

**Important:**
- Do NOT start implementation
- Only update plan/notes docs
- Keep the commit focused on documentation changes only
