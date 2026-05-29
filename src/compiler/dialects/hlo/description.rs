use crate::compiler::dialects::{
    ArgumentDescription, AttrDescription, DialectDescription, OperationDescription,
    ValueTypeDescription,
};

const DTYPE: AttrDescription = AttrDescription { name: "dtype", ty: "DType" };
const SHAPE: AttrDescription = AttrDescription { name: "shape", ty: "Shape" };
const BLOCK_SHAPE: AttrDescription =
    AttrDescription { name: "block_shape", ty: "KvCacheBlockShape" };
const DIST_CONFIG: AttrDescription =
    AttrDescription { name: "dist.config", ty: "DistConfig" };
const DIST: AttrDescription = AttrDescription { name: "dist", ty: "DistDesc" };
const TOKENS_META: AttrDescription =
    AttrDescription { name: "tokens", ty: "TokenPartitionMeta" };
const POSITIONS_META: AttrDescription =
    AttrDescription { name: "positions", ty: "PositionMeta" };
const KVCACHE_META: AttrDescription =
    AttrDescription { name: "kvcache", ty: "KvCacheMeta" };
const ATTN_MASK: AttrDescription =
    AttrDescription { name: "attn_mask", ty: "AttnMask" };

const TENSOR_ATTRS: &[AttrDescription] = &[DTYPE, SHAPE];
const KVCACHE_ATTRS: &[AttrDescription] = &[DTYPE, BLOCK_SHAPE];
const ATTN_META_ATTRS: &[AttrDescription] =
    &[TOKENS_META, POSITIONS_META, KVCACHE_META, ATTN_MASK];
const NO_ATTRS: &[AttrDescription] = &[];
const DIST_ATTRS: &[AttrDescription] = &[DIST];
const GRAPH_PROPERTIES: &[AttrDescription] = &[DIST_CONFIG];

const VALUE_TYPES: &[ValueTypeDescription] = &[
    ValueTypeDescription { name: "hlo.tensor", attrs: TENSOR_ATTRS },
    ValueTypeDescription { name: "hlo.token", attrs: NO_ATTRS },
    ValueTypeDescription { name: "hlo.attn_meta", attrs: ATTN_META_ATTRS },
    ValueTypeDescription { name: "hlo.kvcache", attrs: KVCACHE_ATTRS },
];

const TENSOR: ArgumentDescription = ArgumentDescription { ty: "tensor" };
const BOOL_TENSOR: ArgumentDescription = ArgumentDescription { ty: "tensor<bool>" };
const TOKEN: ArgumentDescription = ArgumentDescription { ty: "token" };
const KVCACHE: ArgumentDescription = ArgumentDescription { ty: "kvcache" };
const ATTN_META: ArgumentDescription = ArgumentDescription { ty: "attn_meta" };

const BINARY_INPUTS: &[ArgumentDescription] = &[TENSOR, TENSOR];
const TERNARY_INPUTS: &[ArgumentDescription] = &[TENSOR, TENSOR, TENSOR];
const PREFILL_INPUTS: &[ArgumentDescription] =
    &[TENSOR, TENSOR, TENSOR, TENSOR, ATTN_META];
const DECODE_INPUTS: &[ArgumentDescription] = &[TENSOR, TENSOR, TENSOR, ATTN_META];
const ROPE_INPUTS: &[ArgumentDescription] = &[TENSOR, ATTN_META];
const UNARY_INPUTS: &[ArgumentDescription] = &[TENSOR];
const ROUTED_COMM_INPUTS: &[ArgumentDescription] = &[TENSOR, TENSOR];
const TENSOR_OUTPUT: &[ArgumentDescription] = &[TENSOR];
const TENSOR_TENSOR_OUTPUT: &[ArgumentDescription] = &[TENSOR, TENSOR];
const BOOL_TENSOR_OUTPUT: &[ArgumentDescription] = &[BOOL_TENSOR];
const TOKEN_OUTPUT: &[ArgumentDescription] = &[TOKEN];
const KVCACHE_OUTPUT: &[ArgumentDescription] = &[KVCACHE];
const KVCACHE_READ_INPUTS: &[ArgumentDescription] = &[KVCACHE, ATTN_META];
const KVCACHE_WRITE_INPUTS: &[ArgumentDescription] =
    &[KVCACHE, ATTN_META, TENSOR, TENSOR];

const OPERATIONS: &[OperationDescription] = &[
    OperationDescription {
        name: "hlo.matmul",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.gemm",
        attrs: NO_ATTRS,
        operands: TERNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.attn.prefill",
        attrs: NO_ATTRS,
        operands: PREFILL_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.attn.decode",
        attrs: NO_ATTRS,
        operands: DECODE_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.attn.rope",
        attrs: NO_ATTRS,
        operands: ROPE_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.add",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.sub",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.mul",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.div",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.max",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.min",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.neg",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.exp",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.log",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.sqrt",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.rsqrt",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.abs",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.eq",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.ne",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.lt",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.le",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.gt",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cmp.ge",
        attrs: NO_ATTRS,
        operands: BINARY_INPUTS,
        results: BOOL_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cast.f16",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cast.f32",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.arith.cast.i32",
        attrs: NO_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.all_reduce.sum",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.all_reduce.max",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.all_gather",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.reduce_scatter.sum",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.broadcast",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.dispatch",
        attrs: DIST_ATTRS,
        operands: ROUTED_COMM_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.combine",
        attrs: DIST_ATTRS,
        operands: ROUTED_COMM_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.send",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TOKEN_OUTPUT,
    },
    OperationDescription {
        name: "hlo.comm.recv",
        attrs: DIST_ATTRS,
        operands: UNARY_INPUTS,
        results: TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.kvcache.read",
        attrs: NO_ATTRS,
        operands: KVCACHE_READ_INPUTS,
        results: TENSOR_TENSOR_OUTPUT,
    },
    OperationDescription {
        name: "hlo.kvcache.write",
        attrs: NO_ATTRS,
        operands: KVCACHE_WRITE_INPUTS,
        results: KVCACHE_OUTPUT,
    },
];

pub fn description() -> DialectDescription {
    DialectDescription {
        name: "hlo",
        graph_properties: GRAPH_PROPERTIES,
        value_types: VALUE_TYPES,
        operations: OPERATIONS,
    }
}
