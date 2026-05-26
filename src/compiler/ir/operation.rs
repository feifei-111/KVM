use super::{AttributeMap, ValueIndex};
use crate::compiler::store::Storable;

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct OpName {
    pub dialect: String,
    pub name: String,
}

impl OpName {
    pub fn new(dialect: impl Into<String>, name: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), name: name.into() };
    }
}

#[derive(Debug)]
pub struct Operation {
    pub name: OpName,
    pub operands: Vec<ValueIndex>,
    pub results: Vec<ValueIndex>,
    pub attrs: AttributeMap,
}

impl Operation {
    pub fn new(
        name: OpName,
        operands: Vec<ValueIndex>,
        results: Vec<ValueIndex>,
        attrs: AttributeMap,
    ) -> Self {
        return Self { name, operands, results, attrs };
    }
}

pub type OperationIndex = super::Index<super::OperationKind>;

impl Storable for Operation {
    type Kind = super::OperationKind;
}
