---
description: Run quality checks and commit if passing
allowed-tools: Bash(just *), Bash(git add:*), Bash(git status:*), Bash(git diff:*), Bash(git commit:*), Read, Edit
---

## Context (auto-gathered)

### Git state
- Branch: !`git branch --show-current`
- Status: !`git status --short`
- Staged changes: !`git diff --cached --stat | tail -10`

### Available tools
- Just commands: !`just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Quality gates

**Standard check (required before commit):**
```bash
just check
```

**Full verification (use for deeper engine changes):**
```bash
just check-full
```

## Your task

1. **Run `just check`** -- fix any failures
2. **If deeper engine or rendering changes**, run `just check-full`
3. **Stage changes** -- prefer specific files over `git add -A`
4. **Commit** following AGENTS.md format:

```
commit-type(Component/File[Subcomponent/method]) Concise description

why: Explanation of necessity or impact.
what:
- Specific technical changes made
- Focused on a single topic
```

**Do NOT commit if checks fail.** Fix issues first.
