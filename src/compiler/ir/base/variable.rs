use std::collections::BTreeSet;

use super::attribute::{AttributeId, AttributeMap};
use super::operator::OperatorId;
use super::ty::IrType;

#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct VariableId(pub usize);

#[derive(Clone, Debug, PartialEq)]
pub struct Variable<T: IrType> {
    pub name: String,
    pub ty: T,
    pub attrs: AttributeMap,
    pub def: Option<OperatorId>,
    pub users: BTreeSet<OperatorId>,
}

impl<T: IrType> Variable<T> {
    pub fn new(name: impl Into<String>, ty: T) -> Self {
        Self {
            name: name.into(),
            ty,
            attrs: AttributeMap::new(),
            def: None,
            users: BTreeSet::new(),
        }
    }

    pub fn with_attr(mut self, name: impl Into<String>, value: AttributeId) -> Self {
        self.attrs.insert(name.into(), value);
        self
    }
}
