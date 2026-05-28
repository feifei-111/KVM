use std::fmt;

use crate::compiler::dialects::kernel::{DIALECT, DType, Shape};
use crate::compiler::ir::{Attr, TypeExpr};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Layout {
    RowMajor,
}

impl Layout {
    fn as_symbol(self) -> &'static str {
        match self {
            Self::RowMajor => "row_major",
        }
    }
}

impl fmt::Display for Layout {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_symbol())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TensorType {
    dtype: DType,
    shape: Shape,
    layout: Layout,
}

impl TensorType {
    pub fn new(dtype: DType, shape: impl Into<Shape>, layout: Layout) -> Self {
        Self { dtype, shape: shape.into(), layout }
    }

    pub fn dtype(&self) -> DType {
        self.dtype
    }

    pub fn shape(&self) -> &Shape {
        &self.shape
    }

    pub fn layout(&self) -> Layout {
        self.layout
    }
}

impl From<TensorType> for TypeExpr {
    fn from(value: TensorType) -> Self {
        TypeExpr::new(DIALECT, "tensor")
            .with_param("dtype", Attr::Symbol(value.dtype.to_string()))
            .with_param("shape", value.shape.as_attr())
            .with_param("layout", Attr::Symbol(value.layout.to_string()))
    }
}
