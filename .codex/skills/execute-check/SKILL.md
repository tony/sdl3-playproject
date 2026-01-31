---
name: execute-check
description: Run quality checks and commit if passing.
---

# Execute Check

Run quality checks and commit if everything passes.

## Context to gather

Run these commands and include the output:

- `git branch --show-current`
- `git status --short`
- `git diff --cached --stat | tail -10`
- `just --summary 2>/dev/null | tr ' ' '\n' | grep -E '^[a-z]' | head -40`

## Quality gates

Standard check (required before commit):

```bash
just check
```

Full verification (use for engine-level changes or CI parity):

```bash
just check-full
```

## Your task

1. Run `just check` -- fix any failures
2. If changes are deep or risky, run `just check-full`
3. Stage changes -- prefer specific files over `git add -A`
4. Commit following AGENTS.md format:

```
commit-type(Component/File[Subcomponent/method]) Concise description

why: Explanation of necessity or impact.
what:
- Specific technical changes made
- Focused on a single topic
```

Commit types: `feat`, `fix`, `refactor`, `docs`, `chore`, `test`, `style`, `js(deps)`, `js(deps[dev])`, `ai(rules[LLM type])`

Do NOT commit if checks fail. Fix issues first.
