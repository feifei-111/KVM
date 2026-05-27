use std::any::Any;

use crate::compiler::store::Storable;

pub type TypeIndex = super::Index<super::TypeKind>;

pub trait Type: std::fmt::Debug {
    fn dialect(&self) -> &str;

    fn kind(&self) -> &str;

    fn identity(&self) -> String {
        return format!("{}::{}", self.dialect(), self.kind());
    }

    fn as_any(&self) -> &dyn Any;
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BuiltinType {
    Unit,
    Index,
    Integer { bits: u16, signed: bool },
    Float { bits: u16 },
}

impl Type for BuiltinType {
    fn dialect(&self) -> &str {
        return "builtin";
    }

    fn kind(&self) -> &str {
        match self {
            BuiltinType::Unit => {
                return "unit";
            }
            BuiltinType::Index => {
                return "index";
            }
            BuiltinType::Integer { .. } => {
                return "integer";
            }
            BuiltinType::Float { .. } => {
                return "float";
            }
        }
    }

    fn identity(&self) -> String {
        match self {
            BuiltinType::Unit => {
                return "builtin::unit".to_string();
            }
            BuiltinType::Index => {
                return "builtin::index".to_string();
            }
            BuiltinType::Integer { bits, signed } => {
                return format!("builtin::integer<{bits},{signed}>");
            }
            BuiltinType::Float { bits } => {
                return format!("builtin::float<{bits}>");
            }
        }
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

impl Storable for Box<dyn Type> {
    type Kind = super::TypeKind;
}
