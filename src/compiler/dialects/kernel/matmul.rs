use crate::compiler::dialects::kernel::{DIALECT, DType};
use crate::compiler::ir::{Attr, TypeExpr};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MatmulType {
    accum: DType,
}

impl MatmulType {
    pub fn new(accum: DType) -> Self {
        Self { accum }
    }

    pub fn accum(&self) -> DType {
        self.accum
    }
}

impl From<MatmulType> for TypeExpr {
    fn from(value: MatmulType) -> Self {
        TypeExpr::new(DIALECT, "matmul")
            .with_param("accum", Attr::Symbol(value.accum.to_string()))
    }
}
