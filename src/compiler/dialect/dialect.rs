use crate::compiler::dialect::Operator;
use crate::compiler::ir::{Attribute, Type};

pub trait Dialect {
    fn name(&self) -> &str;

    fn operators(&self) -> Vec<Box<dyn Operator>> {
        return Vec::new();
    }

    fn types(&self) -> Vec<Box<dyn Type>> {
        return Vec::new();
    }

    fn attributes(&self) -> Vec<Box<dyn Attribute>> {
        return Vec::new();
    }
}
