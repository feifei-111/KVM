use std::any::Any;
use std::collections::BTreeMap;

use super::TypeName;

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct AttrName {
    pub dialect: String,
    pub name: String,
}

impl AttrName {
    pub fn new(dialect: impl Into<String>, name: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), name: name.into() };
    }
}

pub trait Attribute: std::fmt::Debug {
    fn name(&self) -> AttrName;
    fn as_any(&self) -> &dyn Any;
}

#[derive(Clone, Debug)]
pub enum BuiltinAttribute {
    Bool { name: AttrName, value: bool },
    Int { name: AttrName, value: i64 },
    Float { name: AttrName, value: f64 },
    String { name: AttrName, value: String },
    Type { name: AttrName, value: TypeName },
}

impl Attribute for BuiltinAttribute {
    fn name(&self) -> AttrName {
        match self {
            BuiltinAttribute::Bool { name, .. } => {
                return name.clone();
            }
            BuiltinAttribute::Int { name, .. } => {
                return name.clone();
            }
            BuiltinAttribute::Float { name, .. } => {
                return name.clone();
            }
            BuiltinAttribute::String { name, .. } => {
                return name.clone();
            }
            BuiltinAttribute::Type { name, .. } => {
                return name.clone();
            }
        }
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

pub type AttributeMap = BTreeMap<String, Box<dyn Attribute>>;
