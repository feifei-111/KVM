use std::collections::BTreeSet;

use super::{AttributeMap, OperationIndex, TypeIndex};
use crate::compiler::store::Storable;

pub type ValueIndex = super::Index<super::ValueKind>;

#[derive(Debug)]
pub struct Value {
    pub name: String,
    pub ty: TypeIndex,
    pub attrs: AttributeMap,
    pub def: Option<OperationIndex>,
    pub users: BTreeSet<OperationIndex>,
}

impl Value {
    pub fn new(name: impl Into<String>, ty: TypeIndex, attrs: AttributeMap) -> Self {
        let name = name.into();
        return Self { name, ty, attrs, def: None, users: BTreeSet::new() };
    }
}

impl Storable for Value {
    type Kind = super::ValueKind;
}
