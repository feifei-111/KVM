# IR Design

> TL;DR: IR is a syntax layer for a typed value/operation graph. `hlo`, `llo`,
> and future semantic families are dialects over the same graph syntax.

## Goal

The IR module provides a reusable graph syntax for compiler passes. It should
not know KVM-specific high-level or executable semantics directly. Those
semantics are introduced by dialect-defined types and the APIs attached to
those types.

An IR instance is a graph where values and operations are interleaved:

- a graph may own graph-level semantic properties that apply to the whole graph
- a value is data produced by an operation, an input placeholder, or a constant
- an operation consumes values and produces values
- dependencies are derived from operation operands and produced values
- every value and operation has a mandatory type
- every value and operation may also carry attrs

The IR syntax layer must support serialization, deserialization, visualization,
topological traversal, and graph transformation.

## Dialect Boundary

`hlo` and `llo` are not separate IR concepts. They are dialects over the same
IR syntax.

The syntax layer owns generic concepts:

- graph
- graph properties
- value
- operation
- type
- attr
- traversal
- transform
- text format
- visualization

Dialects own semantic concepts:

- tensor type
- buffer type
- matmul operation
- load operation
- barrier operation
- execution scope type
- verifier behavior
- type-specific builder helpers
- type-specific rewrite helpers

This keeps the IR module stable even when dialect semantics change.

## Value And Operation

Value and operation are both typed graph nodes, but they play different syntax
roles.

A value represents an instance flowing through the graph. Its type describes the
semantic category and required value fields. For HLO tensors, dtype and shape
belong to the value type.

An operation represents an action in the graph. There is no separate
`Operator` concept in the core IR: if an operation is a matmul, the operation
is described by an operation name such as `hlo.matmul`. Required operation
settings that do not change the operation family are operation attrs verified
by the dialect.

Operation syntax examples:

```text
hlo.matmul(lhs, rhs)
llo.load_gmem.ca(src)
llo.barrier.block()
```

The exact spelling belongs to the text format and dialect type printers, but
the semantic rule is fixed: semantic operation variants use distinct operation
names; per-instance semantic data and optional metadata stay in attrs.

## Type Versus Attr

Value type and operation type are both mandatory, but they do not need the same
shape.

Value type may be rich. It is the contract for a value and can carry fields
such as:

- tensor dtype and shape
- cache dtype
- buffer memory space in lower-level dialects

Operation type should stay comparatively light. It selects the operation family
and dialect hook:

- operation kind
- operation variants that change behavior enough to deserve a distinct name
- verifier/lowering hook selection

Operation attrs/properties carry per-operation instance data:

- distributed participant description
- precision or algorithm choices when they should not split the operation name
- source locations
- debug names
- pass annotations
- analysis results
- scheduling hints

If removing an operation attr makes an operation ill-defined, the node should
fail dialect verification. That does not make the attr part of the operation
type.

Whole-graph contracts follow the same rule at graph scope. For example, a
distributed execution configuration belongs to the graph because communication
operations are interpreted against one global rank placement. The per-operation
distributed participant description belongs to the communication operation attrs
because it configures that operation instance.

## Type APIs

Types are not just labels. A type selects differentiated dialect APIs. Value
details can be read from value type fields; operation instance settings can be
read from operation attrs.

Examples:

```text
value.type().as_tensor().shape()
operation.type().is("hlo.matmul")
operation.attrs()["dist"]
```

The core IR does not need to know every dialect type. It only needs to provide
safe ways to inspect a node type and ask dialect code for type-specific
behavior.

## Graph Transform Model

Graph transforms operate on public values and operations.

Required transform capabilities:

- replace a value with another value
- replace an operation with another operation
- replace a local subgraph with a new local subgraph
- remove dead operations after replacement
- preserve attrs when the transform requests it
- validate that replacements keep use-def links well formed

Transforms are graph edits, not textual edits. Serialization happens after the
graph is already valid.

## Traversal Model

Topological traversal is defined over operation dependencies.

An operation depends on the defining operations of its operand values. Inputs
and constants have no defining operation, so they do not add dependencies.

The traversal API should report cycles instead of silently choosing an order.
Cycles may be legal in a future control-flow dialect only if that dialect
introduces explicit region/control syntax. The base graph syntax treats cycles
as invalid for topological traversal.

## Text Format

The text format should be stable enough for tests and debugging. It does not
need to clone MLIR syntax, but it should keep the same useful properties:

- every value has a printable name
- every operation has a printable name
- operation operands and results are explicit
- value types are explicit
- operation names are explicit
- attrs are explicit, JSON-formatted, and round-trippable
- dialect-specific type bodies are printable strings or structured records

The text format is the interchange/debug format. The in-memory API is the source
of truth for compiler passes.

## Visualization

Visualization is a debugging API, not a semantic dependency.

The core IR can provide a simple terminal-readable text view, but concrete
visual targets such as Graphviz DOT, PNG, and interactive HTML live in
`src/render`. Renderers depend on IR graph queries; IR does not depend on a
renderer.

Visualization should show:

- operations
- values
- producer edges
- operand edges
- node types
- selected attrs when requested

## Non-Goals

The IR syntax layer should not:

- decide whether a graph is `hlo` or `llo` dialect
- encode KVM lowering policy
- schedule execution
- contain a separate operator registry in the core model
- require all dialect types to be known by the core module
- expose raw storage layout as the public API
