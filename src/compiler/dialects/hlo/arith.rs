use std::fmt;

use crate::compiler::dialects::hlo::{DIALECT, DType};
use crate::compiler::ir::TypeExpr;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ArithBinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Max,
    Min,
}

impl fmt::Display for ArithBinaryOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Add => "add",
            Self::Sub => "sub",
            Self::Mul => "mul",
            Self::Div => "div",
            Self::Max => "max",
            Self::Min => "min",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArithBinaryType {
    op: ArithBinaryOp,
}

impl ArithBinaryType {
    pub fn new(op: ArithBinaryOp) -> Self {
        Self { op }
    }

    pub fn op(&self) -> ArithBinaryOp {
        self.op
    }
}

impl From<ArithBinaryType> for TypeExpr {
    fn from(value: ArithBinaryType) -> Self {
        TypeExpr::new(DIALECT, format!("arith.{}", value.op))
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ArithUnaryOp {
    Neg,
    Exp,
    Log,
    Sqrt,
    Rsqrt,
    Abs,
}

impl fmt::Display for ArithUnaryOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Neg => "neg",
            Self::Exp => "exp",
            Self::Log => "log",
            Self::Sqrt => "sqrt",
            Self::Rsqrt => "rsqrt",
            Self::Abs => "abs",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArithUnaryType {
    op: ArithUnaryOp,
}

impl ArithUnaryType {
    pub fn new(op: ArithUnaryOp) -> Self {
        Self { op }
    }

    pub fn op(&self) -> ArithUnaryOp {
        self.op
    }
}

impl From<ArithUnaryType> for TypeExpr {
    fn from(value: ArithUnaryType) -> Self {
        TypeExpr::new(DIALECT, format!("arith.{}", value.op))
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ArithComparePredicate {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
}

impl fmt::Display for ArithComparePredicate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Eq => "eq",
            Self::Ne => "ne",
            Self::Lt => "lt",
            Self::Le => "le",
            Self::Gt => "gt",
            Self::Ge => "ge",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArithCompareType {
    predicate: ArithComparePredicate,
}

impl ArithCompareType {
    pub fn new(predicate: ArithComparePredicate) -> Self {
        Self { predicate }
    }

    pub fn predicate(&self) -> ArithComparePredicate {
        self.predicate
    }
}

impl From<ArithCompareType> for TypeExpr {
    fn from(value: ArithCompareType) -> Self {
        TypeExpr::new(DIALECT, format!("arith.cmp.{}", value.predicate))
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArithCastType {
    to: DType,
}

impl ArithCastType {
    pub fn new(to: DType) -> Self {
        Self { to }
    }

    pub fn to(&self) -> DType {
        self.to
    }
}

impl From<ArithCastType> for TypeExpr {
    fn from(value: ArithCastType) -> Self {
        TypeExpr::new(DIALECT, format!("arith.cast.{}", value.to))
    }
}
