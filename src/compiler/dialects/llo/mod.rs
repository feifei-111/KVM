//! KVM lower-level operations dialect.
//!
//! This dialect is reserved for the lowered operations consumed by the KVM
//! executor/codegen path. It intentionally has no public operation API yet.

use crate::compiler::dialects::{
    DialectDescription, OperationDescription, ValueTypeDescription,
};

const VALUE_TYPES: &[ValueTypeDescription] = &[];
const OPERATIONS: &[OperationDescription] = &[];

pub fn description() -> DialectDescription {
    DialectDescription {
        name: "llo",
        graph_properties: &[],
        value_types: VALUE_TYPES,
        operations: OPERATIONS,
    }
}
