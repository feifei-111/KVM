use std::any::Any;
use std::collections::BTreeMap;

use crate::compiler::store::Storable;

pub type AttributeIndex = super::Index<super::AttributeKind>;

pub trait Attribute: std::fmt::Debug {
    fn dialect(&self) -> &str;

    fn kind(&self) -> &str;

    fn identity(&self) -> String {
        return format!("{}::{}", self.dialect(), self.kind());
    }

    fn as_any(&self) -> &dyn Any;
}

#[derive(Clone, Debug)]
pub enum BuiltinAttribute {
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    Array(Vec<AttributeIndex>),
    Map(BTreeMap<String, AttributeIndex>),
}

impl Attribute for BuiltinAttribute {
    fn dialect(&self) -> &str {
        return "builtin";
    }

    fn kind(&self) -> &str {
        match self {
            BuiltinAttribute::Bool(_) => {
                return "bool";
            }
            BuiltinAttribute::Int(_) => {
                return "int";
            }
            BuiltinAttribute::Float(_) => {
                return "float";
            }
            BuiltinAttribute::String(_) => {
                return "string";
            }
            BuiltinAttribute::Array(_) => {
                return "array";
            }
            BuiltinAttribute::Map(_) => {
                return "map";
            }
        }
    }

    fn identity(&self) -> String {
        match self {
            BuiltinAttribute::Bool(value) => {
                return format!("builtin::bool<{value}>");
            }
            BuiltinAttribute::Int(value) => {
                return format!("builtin::int<{value}>");
            }
            BuiltinAttribute::Float(value) => {
                return format!("builtin::float<{value}>");
            }
            BuiltinAttribute::String(value) => {
                return format!("builtin::string<{value}>");
            }
            BuiltinAttribute::Array(values) => {
                let values = values
                    .iter()
                    .map(|value| value.raw().to_string())
                    .collect::<Vec<_>>()
                    .join(",");
                return format!("builtin::array<{values}>");
            }
            BuiltinAttribute::Map(values) => {
                let values = values
                    .iter()
                    .map(|(key, value)| format!("{key}:{}", value.raw()))
                    .collect::<Vec<_>>()
                    .join(",");
                return format!("builtin::map<{values}>");
            }
        }
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

pub type AttributeMap = BTreeMap<String, AttributeIndex>;

impl Storable for Box<dyn Attribute> {
    type Kind = super::AttributeKind;
}
