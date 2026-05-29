# Dialect Design

> TL;DR: Dialects are users of the core IR syntax. `hlo` owns high-level
> operation semantics; `llo` is reserved for lowered operation semantics.

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

A dialect should not introduce a second graph representation. It should
construct or wrap core IR values, operations, and types with dialect-specific
APIs.

## Interface Rule

The public dialect interface describes:

- graph-level semantic properties owned by the IR graph
- value type names and required value type fields
- operation names
- required operation attrs
- operation input value types
- operation output value types

It does not expose a separate parameter layer for operations. If a semantic
variant changes the operation definition, use a distinct operation name. If
structured data defines a value, put it in the value type. If structured data
configures a particular operation instance, put it in operation attrs and let
the dialect verifier require it. If it is runtime data flowing through the
graph, make it an input value.

Examples:

```text
hlo.arith.cmp.lt: tensor, tensor -> tensor<bool>
hlo.arith.cast.f16: tensor -> tensor
hlo.comm.all_reduce.sum { dist: DistDesc }: tensor -> tensor
hlo.kvcache.write: kvcache, attn_meta, tensor, tensor -> kvcache
```

## Current Dialects

### hlo

`hlo` is the high-level operations dialect. It describes user-visible compute,
communication, and KV cache semantics that KVM should compile.

Current implemented surface:

- `TensorType`
- `MatmulType`
- `GemmType`
- attention operations
- arith operations
- communication operations
- KV cache value type and read/write operations
- `HloBuilder`
- `hlo::description`

See [hlo/DESIGN.md](hlo/DESIGN.md).

### llo

`llo` is reserved for lower-level operations consumed by the KVM executor or
codegen path.

It is intentionally a placeholder for now.

## Rule Of Thumb

If a concept is about graph mechanics, it belongs in `ir`.

If a concept names a domain object, validates operation legality, infers result
types, or gives typed behavior to a value/operation/type, it belongs in a
dialect.
