# IR Public API

> TL;DR: This is a language-neutral interface for a typed value/operation graph.
> The API describes IR syntax capabilities, not Rust storage or trait design.

This document defines the public interface that users of the IR module should
think in terms of. It intentionally avoids Rust-specific details such as
generics, lifetimes, trait objects, ids, arenas, or indexes.

`kernel`, `executor`, and future semantic families are dialects over this same
IR syntax. They are not separate IR systems.

## Core Terms

### Graph

A graph is one IR unit. It contains values, operations, types, and attrs.

The graph owns the use-def relation:

- an operation consumes values
- an operation produces values
- a value may have one producer operation
- graph inputs and constants have no producer operation
- a value may be used by many operations

### Value

A value is a typed object flowing through the graph.

A value has:

- a mandatory type
- optional attrs
- an optional producer operation
- zero or more user operations
- an optional printable name in text form

The printable name is only for text format and debugging. It is not the semantic
identity of the value.

### Operation

An operation is a typed graph action.

An operation has:

- a mandatory type
- operand values
- result values
- optional attrs
- an optional printable name in text form

The operation type is the operator. There is no separate operator object in the
core IR interface.

### Type

A type is the mandatory semantic contract of a value or operation. A node
without a type is not a complete IR node.

A type has:

- a dialect namespace
- a kind inside that dialect
- semantic parameters
- type-specific behavior exposed by the dialect

Examples:

```text
kernel.tensor<dtype=f16, shape=[128,128], layout=row_major>
kernel.matmul<accum=f32>
executor.load_gmem<bytes=128, cache=ca>
executor.barrier<scope=block>
```

The first example can be used as a value type. The remaining examples can be
used as operation types.

### Attr

An attr is optional metadata attached to a value or operation.

Attrs are the "maybe have" layer of the IR. A graph may use attrs for debug
information, pass annotations, analysis output, or removable hints, but a legal
node should not depend on an attr to answer what it is.

If removing a field makes a value or operation ill-defined, that field belongs
in type. If removing it only loses extra information, that field belongs in
attrs.

## Type Interface

The type interface is the boundary where dialect semantics attach to the
generic graph syntax.

Every type exposes these basic fields:

```text
type.dialect -> DialectName
type.kind    -> KindName
type.params  -> structured parameters
```

`kind` means the category name inside a dialect, for example `tensor`, `matmul`,
`load_gmem`, or `barrier`.

Dialect types may expose additional behavior:

```text
verify_value(graph, value) -> ok | error
verify_operation(graph, operation) -> ok | error
infer_result_types(graph, operation) -> [type] | error
print_type(type) -> text
parse_type(text) -> type | error
```

The core IR does not need to know the full list of dialect-specific behaviors.
It only needs a uniform way to ask for the type of a value or operation and then
let dialect code interpret that type.

## Graph Construction

The graph construction interface should provide these capabilities:

```text
new_graph() -> graph

graph.define_type(type_expression) -> type

graph.add_input(
    type,
    attrs = {}
) -> value

graph.add_constant(
    type,
    attrs = {}
) -> value

graph.add_operation(
    type,
    operands = [value...],
    result_types = [type...],
    attrs = {}
) -> operation
```

Result values are created by `add_operation` from `result_types`. Users query
them from the operation:

```text
graph.results(operation) -> [value...]
```

This keeps the rule simple: operation results are values, and every result value
knows its producer operation.

## Graph Queries

The graph query interface should provide these capabilities:

```text
graph.type_of(value) -> type
graph.type_of(operation) -> type

graph.attrs(value) -> attrs
graph.attrs(operation) -> attrs

graph.producer(value) -> operation | none
graph.users(value) -> [operation...]

graph.operands(operation) -> [value...]
graph.results(operation) -> [value...]
```

These queries describe the graph syntax. Dialect-specific interpretation is done
through the returned types.

Example:

```text
op_type = graph.type_of(operation)

if op_type is kernel.matmul<...>:
    op_type.infer_result_types(graph, operation)
```

The exact host-language mechanism for checking "is this a matmul type?" is an
implementation detail. The public concept is that operation behavior is reached
through operation type.

## Attr Access

Values and operations support flexible attrs:

```text
graph.set_attr(value, key, attr)
graph.set_attr(operation, key, attr)

graph.remove_attr(value, key) -> attr | none
graph.remove_attr(operation, key) -> attr | none

graph.get_attr(value, key) -> attr | none
graph.get_attr(operation, key) -> attr | none
```

Attr values should support at least:

```text
unit
bool
integer
float
string
list
dict
```

Attrs must round-trip through the text format, but they remain optional. Passes
may add, remove, or rewrite attrs when doing so does not change the required
semantic contract of the node.

## Verification

Verification has two layers.

Structural verification checks the generic graph syntax:

- every operand belongs to the graph
- every result belongs to exactly one operation
- every result points back to its producer
- every producer/user relation is consistent
- topological traversal can detect cycles
- attrs are representable in the text format

Type verification delegates to dialect-owned type behavior:

- value types verify values
- operation types verify operations
- operation types may check operands, attrs, and result types

Public capability:

```text
graph.verify() -> ok | error
```

## Topological Traversal

Topological traversal visits operations in dependency order.

An operation depends on the producer operations of its operand values. Graph
inputs and constants do not add dependencies.

Public capabilities:

```text
graph.topological_operations() -> [operation...] | cycle_error

graph.walk_topological(visitor) -> ok | cycle_error
```

The traversal guarantee is:

```text
producer(value) appears before every operation that uses value
```

Cycles are reported as errors by the base graph interface. If a future dialect
needs cyclic control flow, it should introduce explicit control/region syntax
instead of relying on implicit value cycles.

## Graph Transform

Transforms are structural edits over graph entities, not textual edits.

Required capabilities:

```text
graph.replace_value(
    old_value,
    new_value
)

graph.replace_operation(
    old_operation,
    new_operation,
    result_mapping = {
        old_result_value -> new_result_value
    }
)

graph.replace_subgraph(
    old_operations = [operation...],
    new_operations = [operation...],
    boundary_mapping = {
        old_boundary_value -> new_boundary_value
    }
)

graph.erase_operation(operation)
graph.erase_dead_operations() -> [operation...]
```

Replacement updates users through explicit value mappings. The IR does not guess
which new result corresponds to which old result.

After a transform, the graph must still satisfy structural verification.

## Serialization

The IR must round-trip through a text format:

```text
parse_graph(text, type_registry) -> graph | error
print_graph(graph) -> text | error
```

The type registry belongs to dialect integration. It parses and prints type
expressions such as:

```text
kernel.tensor<dtype=f16, shape=[128,128]>
kernel.matmul<accum=f32>
```

The graph text format must preserve:

- value references
- operation entries
- operands
- results
- value types
- operation types
- attrs

One acceptable text shape is:

```text
lhs : kernel.tensor<dtype=f16, shape=[128,128]>
rhs : kernel.tensor<dtype=f16, shape=[128,128]>

out = kernel.matmul<accum=f32>(lhs, rhs)
    -> kernel.tensor<dtype=f16, shape=[128,128]>
    { debug.name = "qk_matmul" }
```

Here:

- `lhs`, `rhs`, and `out` are printable value references
- `kernel.matmul<accum=f32>` is the full operation type
- `debug.name` is an attr, not the operation type

## Visualization

The IR must support graph visualization for debugging.

Required capability:

```text
graph.render(format, options) -> text | error
```

Required formats:

- `text`: simple terminal-readable graph drawing
- `dot`: Graphviz DOT

Useful options:

```text
show_types: true | false
show_attrs: true | false
show_values: true | false
show_operations: true | false
```

Visualization has no semantic meaning. It is a view of the graph.

## Error Categories

The public API should distinguish these error categories:

- missing value
- missing operation
- missing type
- type mismatch
- invalid operand
- invalid result
- invalid replacement
- cycle in topological traversal
- parse error
- print error
- verification error

Errors should carry enough graph context to locate the problem in text output or
visualization.

## Minimal Interaction Example

```text
graph = new_graph()

tensor = graph.define_type(
    kernel.tensor<dtype=f16, shape=[128,128], layout=row_major>
)

lhs = graph.add_input(tensor)
rhs = graph.add_input(tensor)

matmul_type = graph.define_type(kernel.matmul<accum=f32>)

matmul = graph.add_operation(
    type = matmul_type,
    operands = [lhs, rhs],
    result_types = [tensor],
    attrs = { debug.name = "qk" }
)

out = graph.results(matmul)[0]

graph.verify()
graph.topological_operations()
print_graph(graph)
graph.render(dot, show_types = true)
```

The important interface decisions are:

- users manipulate `Value`, `Operation`, and `Type`
- operation type is the operator
- type is the mandatory semantic contract
- dialect semantics attach to types
- attrs are optional metadata
- graph transforms are explicit structural edits
- text and visualization are views over the same graph
