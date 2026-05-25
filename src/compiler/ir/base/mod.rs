pub mod attribute;
pub mod operator;
pub mod variable;

#[path = "type.rs"]
pub mod ty;

pub use attribute::{Attribute, AttributeId, AttributeMap};
pub use operator::{Operator, OperatorId};
pub use ty::{IrType, Type};
pub use variable::{Variable, VariableId};
