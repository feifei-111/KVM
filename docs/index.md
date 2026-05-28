# Documentation Index

This index lists docs that already carry useful project knowledge. Add new
documents only when a design, convention, or investigation has enough shape to
preserve.

| Area | Document | TL;DR |
|------|----------|-------|
| Conventions | [Coding style](conventions/coding-style.md) | Keep code boring, explicit, locally testable, and friendly to profiling. |
| Thinking | [IR system design](thinking/design_ir_system.md) | Core IR stores instances; dialects define semantics; type checking is runtime IR verification. |
| Thinking | [IR open issues](thinking/ir_open_issues.md) | Current IR is semantically clean, but type access and context storage need optimization. |
| Compiler | [Dialect design](../src/compiler/dialects/DESIGN.md) | Dialects are users of core IR syntax and own typed value, operation, and type APIs. |
| Compiler | [Kernel dialect](../src/compiler/dialects/kernel/DESIGN.md) | Kernel is the public high-level dialect for user-visible KVM compile intent. |
