use crate::compiler::ir::{Operator, Type};

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct DialectKey {
    dialect: String,
    kind: String,
}

impl DialectKey {
    pub fn new(dialect: impl Into<String>, kind: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), kind: kind.into() };
    }

    pub fn dialect(&self) -> &str {
        return &self.dialect;
    }

    pub fn kind(&self) -> &str {
        return &self.kind;
    }
}

impl From<&dyn Operator> for DialectKey {
    fn from(operator: &dyn Operator) -> Self {
        return Self::new(operator.dialect(), operator.kind());
    }
}

impl From<&dyn Type> for DialectKey {
    fn from(ty: &dyn Type) -> Self {
        return Self::new(ty.dialect(), ty.kind());
    }
}
