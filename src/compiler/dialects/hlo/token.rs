use crate::compiler::dialects::hlo::DIALECT;
use crate::compiler::ir::TypeExpr;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TokenType;

impl From<TokenType> for TypeExpr {
    fn from(_: TokenType) -> Self {
        TypeExpr::new(DIALECT, "token")
    }
}
