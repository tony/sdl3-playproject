---
name: execute-atomic
description: Implement tasks in separate atomic commits with checks.
---

# Execute Atomic

Implement planned work in separate, check-gated commits.

## Context to gather

Run these commands and include the output:

- `git branch --show-current`
- `git status --short`
- `git log -n 5 --oneline`
- `just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Quality gate

Before **each** commit:

```bash
just check
```

If touching low-level engine or rendering changes, prefer full validation:

```bash
just check-full
```

## Your task

Execute planned work in **separate atomic commits**:

1. Fix any existing check failures first -- commit independently
2. One logical change per commit -- do not bundle unrelated changes
3. Run checks before each commit
4. Follow AGENTS.md commit format:

```
commit-type(Component/File[Subcomponent/method]) Concise description

why: Explanation of necessity or impact.
what:
- Specific technical changes made
- Focused on a single topic
```

Workflow per task:

```
1. Implement change
2. just check
3. (If deeper engine change) just check-full
4. Fix any issues
5. git add <specific files>
6. git commit
7. Repeat for next task
```
