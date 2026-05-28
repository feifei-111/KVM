# Dialect Design

> TL;DR: Dialects are users of the core IR syntax. A dialect owns the typed
> value, operation, and type APIs that give generic IR nodes their semantics.

## Boundary

`src/compiler/ir` defines syntax:

- graph
- value
- operation
- type expression
- attrs
- traversal
- transform
- text format
- visualization

`src/compiler/dialects` defines semantics over that syntax.

A dialect should not introduce a second graph representation. It should wrap or
construct core IR `Value`, `Operation`, and `Type` entities with dialect-specific
APIs.

## Dialect-Owned Concepts

Each dialect owns three semantic layers.

### Type APIs

Types define the must-have semantic contract.

Examples:

```text
kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>
kernel.matmul<accum=f32>
executor.load_gmem<bytes=128, cache=ca>
```

The dialect owns:

- structured constructors for these types
- typed accessors such as dtype, shape, layout, accum dtype
- verification rules for values and operations using those types
- result type inference where useful

### Value APIs

Values are still core IR values, but a dialect may expose typed value wrappers.

Example shape:

```text
KernelTensor(Value)
ExecutorFragment(Value)
ExecutorToken(Value)
```

These wrappers should not own storage. They are typed views over core IR values.
Their job is to make dialect APIs harder to misuse.

### Operation APIs

Operations are still core IR operations, but a dialect may expose typed
operation wrappers or builders.

Example shape:

```text
KernelMatmul(Operation)
ExecutorLoadGmem(Operation)
ExecutorBarrier(Operation)
```

Operation type is the operator. The dialect operation API should construct the
right operation type and result value types, then insert a core IR operation.

## Current Dialects

### kernel

`kernel` is the public high-level dialect. It describes the computation/IO
semantics that users want KVM to compile.

Current implemented surface:

- `TensorType`
- `MatmulType`
- `KernelBuilder`

See [kernel/DESIGN.md](kernel/DESIGN.md).

### executor

`executor` is reserved for lowered operations consumed by the KVM
executor/codegen path.

It is intentionally a placeholder for now.

## Rule Of Thumb

If a concept is about graph mechanics, it belongs in `ir`.

If a concept names a domain object, validates operation legality, infers result
types, or gives typed behavior to a value/operation/type, it belongs in a
dialect.
