# IR Design

> TL;DR: IR is a syntax layer for a typed value/operation graph. `kernel`,
> `executor`, and future semantic families are dialects expressed by types, not
> separate IR systems.

## Goal

The IR module provides a reusable graph syntax for compiler passes. It should
not know KVM-specific high-level or executable semantics directly. Those
semantics are introduced by dialect-defined types and the APIs attached to
those types.

An IR instance is a graph where values and operations are interleaved:

- a value is data produced by an operation, an input placeholder, or a constant
- an operation consumes values and produces values
- dependencies are derived from operation operands and produced values
- every value and operation has a mandatory type
- every value and operation may also carry optional attrs

The IR syntax layer must support serialization, deserialization, visualization,
topological traversal, and graph transformation.

## Dialect Boundary

`kernel` and `executor` are not separate IR concepts. They are dialects over the
same IR syntax.

The syntax layer owns generic concepts:

- graph
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
- matmul operation type
- load operation type
- barrier operation type
- execution scope type
- verifier behavior
- type-specific builder helpers
- type-specific rewrite helpers

This keeps the IR module stable even when kernel/executor semantics change.

## Value And Operation

Value and operation are both typed graph nodes, but they play different syntax
roles.

A value represents an instance flowing through the graph. Its type describes the
semantic category and key properties required to check and use that value.

An operation represents an action in the graph. Its type is the operator. There
is no separate `Operator` concept in the core IR: if an operation is a matmul,
its operation type is the matmul operator type.

Operation type examples:

```text
kernel.matmul(lhs = tensor<f16, 128x128>, rhs = tensor<f16, 128x128>)
executor.load_gmem(bytes = 128, cache = ca)
executor.barrier(scope = block)
```

The exact spelling belongs to the text format and dialect type printers, but
the semantic rule is fixed: operation type is operator identity plus
operator-critical configuration.

## Type Versus Attr

Type is the "must have" part of a value or operation. It is the mandatory
semantic contract that makes the node well defined.

Attr is the "maybe have" part of a value or operation. It stores optional
metadata that can be added, removed, or rewritten without changing what the node
is.

Use type for fields that are required to define the node:

- tensor dtype, shape, layout
- buffer memory space
- operation kind
- operation parameters that change behavior
- execution scope if it changes operation legality or meaning
- verifier-relevant settings

Use attr for optional information:

- debug names
- source locations
- pass annotations
- analysis results
- scheduling hints that a pass may add or remove
- flags that do not change the operation definition

If removing a field makes the value or operation ill-defined, it belongs in
type. If removing it only loses metadata, analysis output, or pass-local
information, it belongs in attr.

Layout follows this rule too. If layout is part of the value contract and
consumers require it to interpret the value correctly, it belongs in type. If it
is only a lowering preference or temporary scheduling hint, it belongs in attr.

## Type APIs

Types are not just labels. A type may expose differentiated APIs through typed
accessors.

Examples:

```text
value.type().as_tensor().shape()
operation.type().as_matmul().infer_result_type(...)
operation.type().as_barrier().sync_scope()
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
- types are explicit
- attrs are explicit and round-trippable
- dialect-specific type bodies are printable strings or structured records

The text format is the interchange/debug format. The in-memory API is the source
of truth for compiler passes.

## Visualization

Visualization is a debugging API, not a semantic dependency.

The first required visualization target is a simple text graph. A Graphviz DOT
export is also acceptable as a stable external format.

Visualization should show:

- operations
- values
- producer edges
- operand edges
- node types
- selected attrs when requested

## Non-Goals

The IR syntax layer should not:

- decide whether a graph is kernel or executor dialect
- encode KVM lowering policy
- schedule execution
- contain a separate operator registry in the core model
- require all dialect types to be known by the core module
- expose raw storage layout as the public API
