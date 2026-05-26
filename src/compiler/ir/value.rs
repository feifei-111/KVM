use std::collections::BTreeSet;

use super::{AttributeMap, OperationIndex, TypeName};
use crate::compiler::store::Storable;

#[derive(Debug)]
pub struct Value {
    pub name: Option<String>,
    pub ty: TypeName,
    pub attrs: AttributeMap,
    pub def: Option<OperationIndex>,
    pub users: BTreeSet<OperationIndex>,
}

impl Value {
    pub fn new(name: Option<String>, ty: TypeName, attrs: AttributeMap) -> Self {
        return Self { name, ty, attrs, def: None, users: BTreeSet::new() };
    }
}

pub type ValueIndex = super::Index<super::ValueKind>;

impl Storable for Value {
    type Kind = super::ValueKind;
}
