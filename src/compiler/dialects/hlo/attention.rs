use std::fmt;

use crate::compiler::dialects::hlo::DIALECT;
use crate::compiler::ir::TypeExpr;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum AttentionOp {
    Prefill,
    Decode,
    Rope,
}

impl fmt::Display for AttentionOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Prefill => "prefill",
            Self::Decode => "decode",
            Self::Rope => "rope",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AttnMetaType;

impl From<AttnMetaType> for TypeExpr {
    fn from(_: AttnMetaType) -> Self {
        TypeExpr::new(DIALECT, "attn_meta")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AttentionType {
    op: AttentionOp,
}

impl AttentionType {
    pub fn new(op: AttentionOp) -> Self {
        Self { op }
    }

    pub fn op(&self) -> AttentionOp {
        self.op
    }
}

impl From<AttentionType> for TypeExpr {
    fn from(value: AttentionType) -> Self {
        TypeExpr::new(DIALECT, format!("attn.{}", value.op))
    }
}
