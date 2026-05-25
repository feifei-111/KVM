---
name: pr-workflow
description: >
  Prepare a KVM change for pull request. Use when the user asks to create,
  prepare, polish, submit, or review a PR; asks how to branch or commit; or
  wants the change packaged for GitHub. Covers branch naming, Commitizen
  commits, local checks, self-review, and PR template content.
---

# PR Workflow

Use this skill when packaging KVM work for review.

## Before the PR

1. Check state:
   - `git status --short --branch`
   - `git diff --stat`
   - `git diff`
2. Confirm the branch is not `main`. If it is, create a topic branch:
   - `feat/<short-name>` for new behavior
   - `fix/<short-name>` for bug fixes
   - `docs/<short-name>` for documentation
   - `chore/<short-name>` for maintenance
   - `refactor/<short-name>`, `perf/<short-name>`, or `test/<short-name>` when
     those are more precise
3. Run checks that match the change. If no project-specific checks exist yet,
   run `pre-commit run --all-files` when available, or inspect changed
   Markdown and scripts manually if pre-commit is not installed.
4. Self-review the diff and fix obvious issues before opening the PR.

## Commit

Use Commitizen-style subjects:

```text
<type>(<scope>): <subject>
```

Examples:

```text
docs(workflow): add project doc skill
feat(executor): add trace replay prototype
fix(planner): reject cyclic task dependencies
```

Do not mix unrelated changes into one commit unless the user explicitly asks for
a checkpoint commit.

## PR Body

Use `.github/PULL_REQUEST_TEMPLATE.md`. Include:

- a summary of behavior or documentation changes
- exact checks run and their result
- risks or open questions
- follow-up work, if any

If using `gh pr create`, prefer a non-interactive command with an explicit title
and body file.
