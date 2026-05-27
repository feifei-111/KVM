use std::any::Any;

use crate::compiler::context::Context;
use crate::compiler::ir::{
    Operation, OperationIndex, Operator, TypeIndex, ValueIndex, first_operand_type,
    require_same_operand_and_result_types,
};

#[derive(Debug)]
pub struct AddOp;

impl AddOp {
    pub const DIALECT: &'static str = "hir";
    pub const KIND: &'static str = "add";

    pub fn lhs(operation: &Operation) -> Option<ValueIndex> {
        return operation.operands.first().copied();
    }

    pub fn rhs(operation: &Operation) -> Option<ValueIndex> {
        return operation.operands.get(1).copied();
    }

    pub fn result(operation: &Operation) -> Option<ValueIndex> {
        return operation.results.first().copied();
    }
}

impl Operator for AddOp {
    fn dialect(&self) -> &str {
        return Self::DIALECT;
    }

    fn kind(&self) -> &str {
        return Self::KIND;
    }

    fn infer_results(
        &self,
        context: &Context,
        operands: &[ValueIndex],
        explicit_results: Vec<TypeIndex>,
    ) -> Result<Vec<TypeIndex>, String> {
        if !explicit_results.is_empty() {
            return Ok(explicit_results);
        }

        return Ok(vec![first_operand_type(context, operands)?]);
    }

    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String> {
        let operation = context
            .operation(operation)
            .ok_or_else(|| "operation does not exist".to_string())?;

        if operation.operands.len() != 2 {
            return Err("hir.add requires exactly two operands".to_string());
        }

        if operation.results.len() != 1 {
            return Err("hir.add requires exactly one result".to_string());
        }

        return require_same_operand_and_result_types(context, operation);
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}
