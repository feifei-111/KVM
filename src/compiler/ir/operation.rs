use super::{AttributeMap, ValueIndex};
use crate::compiler::dialect::OperatorIndex;
use crate::compiler::store::Storable;

pub type OperationIndex = super::Index<super::OperationKind>;

#[derive(Debug)]
pub struct Operation {
    pub operator: OperatorIndex,
    pub operands: Vec<ValueIndex>,
    pub results: Vec<ValueIndex>,
    pub attrs: AttributeMap,
}

impl Operation {
    pub fn new(
        operator: OperatorIndex,
        operands: Vec<ValueIndex>,
        results: Vec<ValueIndex>,
        attrs: AttributeMap,
    ) -> Self {
        return Self { operator, operands, results, attrs };
    }
}

impl Storable for Operation {
    type Kind = super::OperationKind;
}
