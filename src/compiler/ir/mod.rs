mod attr;
mod entity;
mod error;
mod graph;
mod text;
mod ty;

pub use attr::{Attr, AttrMap, format_attr_map, format_attr_map_pretty};
pub use entity::{Block, Operation, Type, Value};
pub use error::IrError;
pub use graph::{
    BlockView, Graph, OperationView, RenderFormat, RenderOptions, ValueKind, ValueView,
};
pub use ty::TypeExpr;

#[cfg(test)]
mod tests;
