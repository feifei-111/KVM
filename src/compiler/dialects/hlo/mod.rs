mod arith;
mod attention;
mod builder;
mod communication;
mod description;
mod dtype;
mod gemm;
mod kvcache;
mod matmul;
mod shape;
mod tensor;
mod token;

pub use arith::{
    ArithBinaryOp, ArithBinaryType, ArithCastType, ArithComparePredicate,
    ArithCompareType, ArithUnaryOp, ArithUnaryType,
};
pub use attention::{AttentionOp, AttentionType, AttnMetaType};
pub use builder::HloBuilder;
pub use communication::{
    BroadcastType, CollectiveOp, CollectiveType, CombineType, DispatchType, DistConfig,
    DistDesc, RankPlacement, RecvType, SendType, set_dist_config,
};
pub use description::description;
pub use dtype::DType;
pub use gemm::GemmType;
pub use kvcache::{KvCacheAccess, KvCacheBlockShape, KvCacheOpType, KvCacheType};
pub use matmul::MatmulType;
pub use shape::Shape;
pub use tensor::TensorType;
pub use token::TokenType;

pub const DIALECT: &str = "hlo";

#[cfg(test)]
mod tests;
