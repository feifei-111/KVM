# Contributing to KVM

KVM explores a fine-grained kernel virtual machine. Contributions should keep
the planner, profiler, IR, and executor boundaries explicit.

## Before You Start

Read [docs/index.md](docs/index.md) when your change touches docs or project
conventions. Create new design docs only when there is a concrete decision,
prototype, or investigation worth preserving.

## Branches

Use a topic branch instead of committing directly to `main`.

- `feat/` for new behavior
- `fix/` for bug fixes
- `docs/` for documentation-only changes
- `chore/` for maintenance
- `refactor/` for structure-preserving code changes
- `perf/` for performance work
- `test/` for test-only changes

## Commits

Use Commitizen-style commit messages:

```text
<type>(<scope>): <subject>
```

Examples:

```text
docs(conventions): document review expectations
feat(ir): add task schema prototype
test(profiler): cover trace scoring
```

## Pull Requests

Before opening a PR:

1. Run the checks that match your change.
2. Self-review the diff.
3. Update docs when behavior, design, or workflow changed.
4. Fill out the PR template with summary, tests, risks, and follow-ups.

## Pre-Commit

Install hooks with:

```bash
pre-commit install
```

Run all hooks manually with:

```bash
pre-commit run --all-files
```
