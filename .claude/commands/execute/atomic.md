---
description: Implement tasks in separate atomic commits with checks
allowed-tools: Bash(just *), Bash(git *), Read, Edit, Glob, Grep, Task
---

## Context (auto-gathered)

### Git state
- Branch: !`git branch --show-current`
- Status: !`git status --short`
- Recent commits: !`git log -n 5 --oneline`

### Available tools
- Just commands: !`just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Quality gate

Before **each** commit:
```bash
just check
```

If touching low-level engine or rendering changes:
```bash
just check-full
```

## Your task

Execute planned work in **separate atomic commits**:

1. Fix any existing check failures first -- commit independently
2. One logical change per commit -- don't bundle unrelated changes
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
