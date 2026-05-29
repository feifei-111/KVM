use crate::compiler::dialects::hlo::{DIALECT, DType, Shape};
use crate::compiler::ir::{Attr, TypeExpr};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TensorType {
    dtype: DType,
    shape: Shape,
}

impl TensorType {
    pub fn new(dtype: DType, shape: impl Into<Shape>) -> Self {
        Self { dtype, shape: shape.into() }
    }

    pub fn dtype(&self) -> DType {
        self.dtype
    }

    pub fn shape(&self) -> &Shape {
        &self.shape
    }
}

impl From<TensorType> for TypeExpr {
    fn from(value: TensorType) -> Self {
        TypeExpr::new(DIALECT, "tensor")
            .with_field("dtype", Attr::Symbol(value.dtype.to_string()))
            .with_field("shape", value.shape.as_attr())
    }
}
