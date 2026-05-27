use std::any::Any;

use crate::compiler::context::Context;
use crate::compiler::ir::{OperationIndex, OperatorKind, TypeIndex, ValueIndex};
use crate::compiler::store::Storable;

pub type OperatorIndex = crate::compiler::ir::Index<OperatorKind>;

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

#[derive(Debug)]
pub struct AnyOperator {
    dialect: String,
    kind: String,
}

impl AnyOperator {
    pub fn new(dialect: impl Into<String>, kind: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), kind: kind.into() };
    }
}

impl Operator for AnyOperator {
    fn dialect(&self) -> &str {
        return &self.dialect;
    }

    fn kind(&self) -> &str {
        return &self.kind;
    }

    fn verify(
        &self,
        _context: &Context,
        _operation: OperationIndex,
    ) -> Result<(), String> {
        return Ok(());
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

#[derive(Clone, Debug)]
pub struct SameOperandsAndResultsType {
    dialect: String,
    kind: String,
}

impl SameOperandsAndResultsType {
    pub fn new(dialect: impl Into<String>, kind: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), kind: kind.into() };
    }
}

impl Operator for SameOperandsAndResultsType {
    fn dialect(&self) -> &str {
        return &self.dialect;
    }

    fn kind(&self) -> &str {
        return &self.kind;
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

        let first = first_operand_type(context, operands)?;
        return Ok(vec![first]);
    }

    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String> {
        let operation = context
            .operation(operation)
            .ok_or_else(|| "operation does not exist".to_string())?;

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
                return Err(
                    "all results must have the same type as operands".to_string()
                );
            }
        }

        return Ok(());
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

fn first_operand_type(
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
