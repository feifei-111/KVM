# HLO Dialect

> TL;DR: `hlo` is the high-level operations dialect. It describes user-visible
> compute, communication, and KV cache semantics over the generic IR graph.

## Role

`hlo` is the semantic layer above the core IR syntax. It should express what
the compiler needs to lower, without committing to low-level instruction
selection or an executor schedule.

Example:

```text
%lhs : hlo.tensor<dtype=f16, shape=[128,128]>
%rhs : hlo.tensor<dtype=f16, shape=[128,128]>

%out = "hlo.matmul"(%lhs, %rhs)
    : (hlo.tensor, hlo.tensor) -> hlo.tensor<dtype=f16, shape=[128,128]>
```

## Naming Rule

`hlo` is the dialect name. Names such as `arith`, `comm`, and `kvcache` are only
operation-name namespaces inside the dialect.

Operation semantic variants are expressed by different operation names. For
example, comparison predicates and cast destinations are not hidden fields:

```text
hlo.arith.cmp.lt: tensor, tensor -> tensor<bool>
hlo.arith.cast.f16: tensor -> tensor
```

If something is dynamic data, it should be an input value. If something changes
the operation definition, it should be part of the operation name.

## Current Surface

Value types:

```text
hlo.tensor<dtype: DType, shape: Shape>
hlo.token
hlo.attn_meta {
  tokens: TokenPartitionMeta,
  positions: PositionMeta,
  kvcache: KvCacheMeta,
  attn_mask: AttnMask
}
hlo.kvcache<dtype: DType, block_shape: KvCacheBlockShape>
```

Representative operations:

```text
hlo.matmul: tensor, tensor -> tensor
hlo.gemm: tensor, tensor, tensor -> tensor
hlo.attn.prefill: tensor, tensor, tensor, tensor, attn_meta -> tensor
hlo.attn.decode: tensor, tensor, tensor, attn_meta -> tensor
hlo.attn.rope: tensor, attn_meta -> tensor
hlo.arith.add: tensor, tensor -> tensor
hlo.arith.cmp.lt: tensor, tensor -> tensor<bool>
hlo.arith.cast.f16: tensor -> tensor
hlo.comm.all_reduce.sum { dist: DistDesc }: tensor -> tensor
hlo.comm.broadcast { dist: DistDesc }: tensor -> tensor
hlo.comm.dispatch { dist: DistDesc }: tensor, tensor -> tensor
hlo.comm.combine { dist: DistDesc }: tensor, tensor -> tensor
hlo.comm.send { dist: DistDesc }: tensor -> token
hlo.comm.recv { dist: DistDesc }: tensor -> tensor
hlo.kvcache.read: kvcache, attn_meta -> tensor, tensor
hlo.kvcache.write: kvcache, attn_meta, tensor, tensor -> kvcache
```

Distributed communication is described in two layers:

- the graph owns `dist.config`, which describes the global distributed world
  with backend, global ranks, and rank placement
- each communication operation owns a required `dist` attr, which describes the
  participating global ranks for that operation

HLO communication is device-to-device tensor communication. The IR uses global
ranks only. Group-local ranks, NCCL communicators, CUDA streams, and other
runtime handles are derived while lowering or executing the graph; they are not
HLO values.

`token` is only an ordering value for side-effecting operations. It is not a
device buffer. `send` uses its tensor operand as the local source buffer, and
`recv` uses its tensor operand as the local destination buffer.

`comm.dispatch` and `comm.combine` take both payload data and routing data as
tensor operands. The routing tensor is data flow, not an attr or hidden
parameter.

`matmul` models a matrix multiply. `gemm` models a fused matrix multiply with an
explicit accumulator/bias input. That third operand is data flow, not an attr.
`attn.prefill` and `attn.decode` are separate operation names because they
lower differently. Attention position/cache indexing metadata is represented as
`hlo.attn_meta`, not as a position embedding tensor. Position embedding/RoPE is
an explicit `hlo.attn.rope` operation over Q/K tensors and the attention
metadata.

`hlo.attn_meta` is a value because it flows through attention and KV cache
operations. Its attrs describe the metadata schema for the value:

- `tokens.seq_offsets`: how the packed token dimension is split into sequences
- `positions.start_positions`: each sequence's current chunk start position for
  RoPE
- `kvcache.block_size`: token count per KV cache block
- `kvcache.block_indices`: per-sequence KV cache block ids
- `kvcache.seq_lengths`: each sequence's current visible KV length
- `attn_mask`: attention mask mode, one of `none`, `causal`, or `masked`

When `attn_mask` is `masked`, the attention operation's mask tensor operand is
semantically used. When it is `causal`, the causal mask is implied by positions
and sequence lengths. When it is `none`, no attention mask is applied.

It is not an embedding tensor and should not be treated as an intermediate
activation.

KV cache allocation is not modeled in HLO. HLO sees a cache value and only
models semantic reads and writes. A read produces cached K and V tensors. A
write stores current K and V tensors.

`hlo.kvcache.block_shape` describes the logical block storage convention. The
initial supported value is `paged_vllm`, matching the paged KV cache family
where cache storage is organized around block ids and a block holds
`block_size` tokens for K/V heads. `hlo.attn_meta.kvcache` then describes how
the current batch indexes those blocks.

## Boundary

`hlo` owns high-level semantic APIs:

- tensor, token, attention metadata, and KV cache value types
- operation names and builders
- future HLO verification and result inference

`hlo` does not own:

- graph storage
- use-def maintenance
- topological traversal
- generic graph transforms
- text rendering mechanics
- lower-level scheduling decisions

Those belong to core IR or later dialects such as `llo`.
