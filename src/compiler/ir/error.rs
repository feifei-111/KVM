use std::fmt;

use super::{Operation, Type, Value};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum IrError {
    MissingValue(Value),
    MissingOperation(Operation),
    MissingType(Type),
    InvalidOperand(String),
    InvalidReplacement(String),
    Cycle(Vec<Operation>),
    Parse(String),
    Verify(String),
}

impl fmt::Display for IrError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::MissingValue(value) => write!(f, "missing value {value:?}"),
            Self::MissingOperation(op) => write!(f, "missing operation {op:?}"),
            Self::MissingType(ty) => write!(f, "missing type {ty:?}"),
            Self::InvalidOperand(msg) => write!(f, "invalid operand: {msg}"),
            Self::InvalidReplacement(msg) => write!(f, "invalid replacement: {msg}"),
            Self::Cycle(ops) => write!(f, "cycle in operations: {ops:?}"),
            Self::Parse(msg) => write!(f, "parse error: {msg}"),
            Self::Verify(msg) => write!(f, "verify error: {msg}"),
        }
    }
}

impl std::error::Error for IrError {}
