# IR Public API

> TL;DR: This is a language-neutral interface for a typed value/operation graph.
> The API describes IR syntax capabilities, not Rust storage or trait design.

This document defines the public interface that users of the IR module should
think in terms of. It intentionally avoids Rust-specific details such as
generics, lifetimes, trait objects, ids, arenas, or indexes.

Dialect families such as `hlo` are dialects over this same IR syntax. They are
not separate IR systems.

## Core Terms

### Graph

A graph is one IR unit. It contains values, operations, types, and attrs.

A graph may also carry graph-level semantic properties. These properties are
part of the graph, not part of any single operation. Dialects use them for
whole-graph contracts such as distributed execution configuration.

The graph owns the use-def relation:

- an operation consumes values
- an operation produces values
- a value may have one producer operation
- block inputs and constants have no producer operation
- a value may be used by many operations

### Value

A value is a typed object flowing through the graph.

A value has:

- a mandatory type
- attrs
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
- attrs
- an optional printable name in text form

The operation has a mandatory semantic contract. There is no separate operator
object in the core IR interface.

### Block

A block is the callable boundary of a graph. It gives the graph an explicit
ordered interface, similar to a function signature:

- ordered input values
- ordered output values
- a body made of values and operations
- optional attrs
- an optional printable name

Block inputs are the values supplied by the caller of this graph. In a model
execution graph, this usually includes request tensors and model weights,
because both are external values needed to execute the graph. A value should
only be a graph constant when the IR itself owns the literal value or the
dialect defines a concrete constant-loading semantic.

A graph may contain one entry block in the current interface. Future control
flow may introduce multiple blocks or regions, but the first required contract
is a single callable entry block.

### Type

A type is the mandatory semantic contract of a value or operation. A node
without a type is not a complete IR node.

Value types and operation types deliberately have different weight.

A value type has:

- a dialect namespace
- a kind inside that dialect
- required value fields, such as tensor dtype and shape
- type-specific behavior exposed by the dialect

An operation type has:

- a dialect namespace
- a kind inside that dialect
- operation-specific hooks exposed by the dialect

Operation type should stay comparatively light. Per-operation instance settings
such as distributed rank selection or precision policy should be represented as
operation attrs unless they are important enough to split into a distinct
operation kind.

### Attr

An attr is structured data attached to a graph, block, value, or operation.
In the text/debug surface, attr maps are printed as JSON objects. Type fields
keep their type-printer syntax, for example `dtype=f16`.

Attrs have two roles:

- required instance properties declared and verified by a dialect
- optional metadata such as debug names, analysis output, and pass-local hints

For example, `hlo.tensor<dtype=f16, shape=[8,4096]>` is a value type, while
`hlo.comm.dispatch` is an operation type and its `dist` description is a
required operation attr interpreted against the graph-level `dist.config`
property.

## Type Interface

The type interface is the boundary where dialect semantics attach to the
generic graph syntax.

Every type exposes these basic fields:

```text
type.dialect -> DialectName
type.kind    -> KindName
type.fields  -> structured fields
```

`kind` means the category name inside a dialect, for example `tensor`.

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

graph.set_property(key, attr)
graph.get_property(key) -> attr | none
graph.remove_property(key) -> attr | none

graph.new_block(
    name,
    inputs = [(name, type)...],
    attrs = {}
) -> block

block.input(index) -> value
block.inputs() -> [value...]

graph.add_constant(
    name,
    type,
    attrs = {}
) -> value

graph.add_operation(
    operation,
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

`add_constant` is for values whose literal contents or loading semantics are
owned by the IR/dialect. Model weights should be block inputs unless the
dialect has defined a real embedded-constant representation.

Blocks own the ordered callable boundary:

```text
block.set_outputs([value...])
block.outputs() -> [value...]
```

The order of `block.inputs()` and `block.outputs()` is semantic. It defines the
function-call ABI of the graph. Output types are the referenced value types.
Printable value names do not define argument order.

## Graph Queries

The graph query interface should provide these capabilities:

```text
graph.type_of(value) -> type
graph.contract_of(operation) -> operation contract
graph.entry_block() -> block

graph.attrs(value) -> attrs
graph.attrs(operation) -> attrs
graph.attrs(block) -> attrs

graph.producer(value) -> operation | none
graph.users(value) -> [operation...]

block.inputs() -> [value...]
block.outputs() -> [value...]

graph.operands(operation) -> [value...]
graph.results(operation) -> [value...]
```

These queries describe the graph syntax. Dialect-specific interpretation is done
through the returned types.

Example:

```text
contract = graph.contract_of(operation)

if contract is hlo.matmul:
    contract.infer_result_types(graph, operation)
```

The exact host-language mechanism for checking "is this a matmul type?" is an
implementation detail. The public concept is that operation behavior is reached
through the operation's mandatory semantic contract.

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

Some operation attrs are required by dialect semantics. Removing a required attr
makes the node fail dialect verification, but it does not become part of the
operation type.

## Verification

Verification has two layers.

Structural verification checks the generic graph syntax:

- the graph has an entry block when it is used as a callable unit
- every block input belongs to the graph
- every block output belongs to the graph
- every block output is either a block input, constant, or operation result
- every declared block output type matches the referenced value type
- every operand belongs to the graph
- every result belongs to exactly one operation
- every result points back to its producer
- every producer/user relation is consistent
- topological traversal can detect cycles
- attrs are representable in the text format

Dialect verification delegates to dialect-owned behavior:

- value types verify values
- operations verify operands, attrs, and result types using their operation
  definition

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

Serialization is a capability of the IR module, not the graph API itself.
Callers should depend on parse/print behavior and semantic preservation, not on
using text as the construction interface.

Required capabilities:

```text
parse_graph(text, type_registry) -> graph | error
print_graph(graph) -> text | error
```

The concrete text grammar is documented separately from this API. The text
format should follow MLIR-style conventions where possible instead of inventing
new notation. Regardless of spelling, serialization must preserve:

- graph-level properties
- block input order
- block output order
- value references
- operation entries
- operands
- results
- value types
- operation semantic contracts
- attrs

Changing the serialization grammar should not change the graph construction,
query, transform, verification, or dialect APIs. Text is an interchange and
debugging surface.

## Visualization

Visualization is a debugging surface built on top of the IR graph. The core IR
module should expose enough graph queries for renderers, while concrete visual
targets live outside the IR module.

Current required render targets:

```text
render.dot(graph, options) -> dot text | error
render.png(graph, path, options) -> ok | error
render.html(graph, path, options) -> ok | error
```

Useful options:

```text
show_types: true | false
show_attrs: true | false
show_values: true | false
show_operations: true | false
```

Visualization has no semantic meaning. It is a view of the graph, and changing
the renderer should not change graph construction, verification, traversal, or
transforms.

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

graph.set_property(
    "dist.config",
    {
        "backend": "nccl",
        "ranks": [
            { "rank": 0, "host": "node0", "process": 0, "device": "cuda:0" },
            { "rank": 1, "host": "node0", "process": 1, "device": "cuda:1" }
        ]
    }
)

tensor = graph.define_type(
    hlo.tensor<dtype=f16, shape=[128,128]>
)

main = graph.new_block(
    "main",
    inputs = [("tokens", tensor), ("weight", tensor)]
)

matmul = graph.add_operation(
    operation = hlo.matmul,
    operands = [main.input(0), main.input(1)],
    result_types = [tensor],
    attrs = { "debug.name": "qk" }
)

out = graph.results(matmul)[0]
main.set_outputs([out])

graph.verify()
graph.topological_operations()
print_graph(graph)
render.dot(graph, show_types = true)
```

The important interface decisions are:

- users manipulate `Value`, `Operation`, and `Type`
- block input order defines graph call argument order
- block output order defines graph call result order
- type is the mandatory semantic contract
- value types may carry required value fields such as dtype and shape
- operation types should stay light and select operation hooks
- required operation attrs/properties hold per-operation instance data
- optional attrs hold metadata
- graph transforms are explicit structural edits
- text and visualization are views over the same graph
