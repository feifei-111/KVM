# Kernel Dialect

> TL;DR: `kernel` is the public high-level dialect. It describes the kernel
> semantics users want KVM to compile, while the core IR only stores the graph
> syntax.

## Role

The kernel dialect is the first semantic layer above the generic IR graph. It
should express user-visible compute and IO intent without committing to a KVM
executor schedule.

Examples:

```text
lhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>
rhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>

out = kernel.matmul<accum=f32>(lhs, rhs)
    -> kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>
```

## Boundary

The kernel dialect owns high-level semantic APIs:

- tensor types
- operation types such as matmul
- typed builders for creating kernel operations
- future verification and result inference for kernel operations

The kernel dialect does not own:

- graph storage
- use-def maintenance
- topological traversal
- generic graph transforms
- text rendering mechanics
- executor scheduling decisions

Those belong to the core IR or later compiler stages.

## Current Surface

Implemented types:

- `DType`
- `Shape`
- `Layout`
- `TensorType`
- `MatmulType`

Implemented builder:

- `KernelBuilder::tensor_type`
- `KernelBuilder::input`
- `KernelBuilder::constant`
- `KernelBuilder::matmul`

## Type And Attr Rule

Kernel types carry must-have semantic contracts.

For example, tensor dtype, shape, and layout are part of
`kernel.tensor<...>` because consumers need them to interpret the value.

Attrs are optional metadata. Debug names, pass annotations, or temporary
analysis results may be attached as attrs, but a kernel value or operation
should remain well-defined if those attrs are removed.
