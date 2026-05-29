use crate::compiler::dialects::hlo::DIALECT;
use crate::compiler::ir::TypeExpr;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GemmType;

impl GemmType {
    pub fn new() -> Self {
        Self
    }
}

impl From<GemmType> for TypeExpr {
    fn from(_: GemmType) -> Self {
        TypeExpr::new(DIALECT, "gemm")
    }
}
