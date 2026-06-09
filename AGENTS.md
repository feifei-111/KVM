# Agent Guide

This repository uses local skills under `.agents/skills/`.

## Required Habits

- Read [docs/index.md](docs/index.md) before changing docs or project
  conventions.
- Do not create design documents before the design has enough shape to be
  useful.
- When a decision becomes concrete, document why it was made, not only what
  changed.
- Before opening a PR, run the relevant local checks and fill out the template.

## Git Conventions

- Work on a topic branch, not directly on `main`.
- Branch prefixes: `feat/`, `fix/`, `docs/`, `chore/`, `refactor/`,
  `perf/`, `test/`.
- Commit messages use Commitizen-style subjects:
  `<type>(<scope>): <subject>`.
