use std::collections::BTreeMap;

#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct AttributeId(pub usize);

#[derive(Clone, Debug, PartialEq)]
pub enum Attribute {
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    List(Vec<AttributeId>),
    Map(AttributeMap),
}

pub type AttributeMap = BTreeMap<String, AttributeId>;

impl From<bool> for Attribute {
    fn from(value: bool) -> Self {
        Self::Bool(value)
    }
}

impl From<i64> for Attribute {
    fn from(value: i64) -> Self {
        Self::Int(value)
    }
}

impl From<f64> for Attribute {
    fn from(value: f64) -> Self {
        Self::Float(value)
    }
}

impl From<String> for Attribute {
    fn from(value: String) -> Self {
        Self::String(value)
    }
}

impl From<&str> for Attribute {
    fn from(value: &str) -> Self {
        Self::String(value.to_string())
    }
}
