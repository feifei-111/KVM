pub mod dialect;
pub mod key;
pub mod op_view;
pub mod operator;

pub use dialect::Dialect;
pub use key::DialectKey;
pub use op_view::OpView;
pub use operator::{AnyOperator, Operator, OperatorIndex, SameOperandsAndResultsType};

#[cfg(test)]
mod tests;
