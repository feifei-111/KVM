use std::any::Any;

use crate::compiler::context::Context;
use crate::compiler::store::Storable;

use super::{Operation, OperationIndex, OperatorKind, TypeIndex, ValueIndex};

pub type OperatorIndex = super::Index<OperatorKind>;

impl Storable for Box<dyn Operator> {
    type Kind = OperatorKind;
}

pub trait Operator: std::fmt::Debug {
    fn dialect(&self) -> &str;

    fn kind(&self) -> &str;

    fn infer_results(
        &self,
        _context: &Context,
        _operands: &[ValueIndex],
        explicit_results: Vec<TypeIndex>,
    ) -> Result<Vec<TypeIndex>, String> {
        return Ok(explicit_results);
    }

    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String>;

    fn as_any(&self) -> &dyn Any;
}

pub fn first_operand_type(
    context: &Context,
    operands: &[ValueIndex],
) -> Result<TypeIndex, String> {
    let first = operands
        .first()
        .ok_or_else(|| "operator requires at least one operand".to_string())?;

    let value = context
        .value(*first)
        .ok_or_else(|| "operand value does not exist".to_string())?;

    return Ok(value.ty);
}

pub fn require_same_operand_and_result_types(
    context: &Context,
    operation: &Operation,
) -> Result<(), String> {
    if operation.operands.is_empty() {
        return Err("operator requires at least one operand".to_string());
    }

    if operation.results.is_empty() {
        return Err("operator requires at least one result".to_string());
    }

    let expected = first_operand_type(context, &operation.operands)?;

    for operand in &operation.operands {
        let actual = context
            .value(*operand)
            .ok_or_else(|| "operand value does not exist".to_string())?
            .ty;
        if actual != expected {
            return Err("all operands must have the same type".to_string());
        }
    }

    for result in &operation.results {
        let actual = context
            .value(*result)
            .ok_or_else(|| "result value does not exist".to_string())?
            .ty;
        if actual != expected {
            return Err("all results must have the same type as operands".to_string());
        }
    }

    return Ok(());
}
