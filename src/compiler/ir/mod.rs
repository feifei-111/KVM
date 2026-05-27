pub mod attribute;
pub mod index;
pub mod operation;
pub mod value;

#[path = "type.rs"]
pub mod ty;

pub use attribute::{Attribute, AttributeIndex, AttributeMap, BuiltinAttribute};
pub use index::{
    AttributeKind, Index, IndexKind, OperationKind, OperatorKind, TypeKind, ValueKind,
};
pub use operation::{Operation, OperationIndex};
pub use ty::{BuiltinType, Type, TypeIndex};
pub use value::{Value, ValueIndex};
