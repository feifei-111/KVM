use crate::compiler::dialects::hlo::DIALECT;
use crate::compiler::ir::TypeExpr;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MatmulType;

impl MatmulType {
    pub fn new() -> Self {
        Self
    }
}

impl From<MatmulType> for TypeExpr {
    fn from(_: MatmulType) -> Self {
        TypeExpr::new(DIALECT, "matmul")
    }
}
