use super::attribute::{AttributeId, AttributeMap};

pub trait IrType: Clone + std::fmt::Debug + PartialEq {}

#[derive(Clone, Debug, PartialEq)]
pub struct Type {
    pub name: String,
    pub attrs: AttributeMap,
}

impl Type {
    pub fn new(name: impl Into<String>) -> Self {
        Self { name: name.into(), attrs: AttributeMap::new() }
    }

    pub fn with_attr(mut self, name: impl Into<String>, value: AttributeId) -> Self {
        self.attrs.insert(name.into(), value);
        self
    }
}

impl IrType for Type {}
