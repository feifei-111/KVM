---
name: project-doc
description: >
  Use KVM docs deliberately. Trigger when the user asks to create or update
  documentation, when a task changes project conventions, or when an
  investigation/design has produced concrete decisions worth preserving. Read
  docs/index.md first and keep docs minimal.
---

# Project Doc

Use this skill when documentation itself is part of the task. Do not force a
new doc for every coding task. KVM docs should grow from decisions,
prototypes, investigations, and repeated workflows.

Skip this skill for pure Q&A, early brainstorming, or implementation work where
no durable knowledge has emerged yet.

## Workflow

### 1. Read Existing Docs

1. Read `docs/index.md`.
2. Read only directly relevant docs.
3. Prefer updating an existing doc over creating a new one.

### 2. Decide Whether A New Doc Is Earned

Create a new doc only if at least one is true:

- a design boundary has become concrete
- a prototype produced reusable findings
- a repeated workflow needs a stable recipe
- a convention needs to be enforced across future work

Do not create placeholder roadmap, subsystem, playbook, or lesson docs just
because a directory structure exists.

### 3. Write Minimal Docs

Use this structure when creating a design or investigation doc:

```markdown
# <Title>

> **TL;DR:** One-line summary.

## Context

## Decision / Finding

## Open Questions
```

Keep it short. Link code, commands, and artifacts only when they actually
support the decision or finding.

### 4. Update The Index

Update `docs/index.md` when creating or deleting a doc, or when an existing row
becomes misleading.
