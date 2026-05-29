use std::fmt;

use crate::compiler::dialects::hlo::{DIALECT, DType};
use crate::compiler::ir::{Attr, TypeExpr};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KvCacheAccess {
    Read,
    Write,
}

impl fmt::Display for KvCacheAccess {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Read => "read",
            Self::Write => "write",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KvCacheBlockShape {
    PagedVllm,
}

impl fmt::Display for KvCacheBlockShape {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::PagedVllm => "paged_vllm",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct KvCacheType {
    dtype: DType,
    block_shape: KvCacheBlockShape,
}

impl KvCacheType {
    pub fn new(dtype: DType) -> Self {
        Self { dtype, block_shape: KvCacheBlockShape::PagedVllm }
    }

    pub fn with_block_shape(mut self, block_shape: KvCacheBlockShape) -> Self {
        self.block_shape = block_shape;
        self
    }

    pub fn dtype(&self) -> DType {
        self.dtype
    }

    pub fn block_shape(&self) -> KvCacheBlockShape {
        self.block_shape
    }
}

impl From<KvCacheType> for TypeExpr {
    fn from(value: KvCacheType) -> Self {
        TypeExpr::new(DIALECT, "kvcache")
            .with_field("dtype", Attr::Symbol(value.dtype.to_string()))
            .with_field("block_shape", Attr::Symbol(value.block_shape.to_string()))
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct KvCacheOpType {
    access: KvCacheAccess,
}

impl KvCacheOpType {
    pub fn new(access: KvCacheAccess) -> Self {
        Self { access }
    }

    pub fn access(&self) -> KvCacheAccess {
        self.access
    }
}

impl From<KvCacheOpType> for TypeExpr {
    fn from(value: KvCacheOpType) -> Self {
        TypeExpr::new(DIALECT, format!("kvcache.{}", value.access))
    }
}
