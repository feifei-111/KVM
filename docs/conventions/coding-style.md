# Coding Style

> **TL;DR:** Keep code boring, explicit, locally testable, and friendly to
> profiling.

## Principles

- Prefer small modules with clear ownership over clever shared abstractions.
- Keep runtime-critical decisions explicit and measurable.
- Make data formats versionable from the start if traces or IR will be stored.
- Avoid hidden global state in planner, profiler, and executor code.
- Keep generated artifacts out of source unless they are deliberately checked in
  as fixtures.

## Profiling-Friendly Code

- Name phases and operations consistently so traces are readable.
- Keep benchmark commands reproducible and document the exact inputs used.
- Separate setup, warmup, and measured execution paths.
- Do not report profiler-instrumented latency as production latency without
  saying how it was captured.

## Documentation

- Update relevant docs when behavior or design intent changes.
- Commands in docs should be verified before they are presented as recipes.
- Capture pitfalls and tradeoffs where future work will look for them, not in
  loose notes.

## Formatting

- Use pre-commit for basic file hygiene.
- Rust formatting follows `rustfmt.toml`.
- Keep line endings as LF.
