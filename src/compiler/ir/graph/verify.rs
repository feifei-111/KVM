use super::Graph;
use crate::compiler::ir::{IrError, Operation, Type, Value};

impl Graph {
    pub fn verify(&self) -> Result<(), IrError> {
        for (index, _) in self.types.iter().enumerate() {
            self.expect_type(Type::new(index))?;
        }
        for (index, value) in self.values.iter().enumerate() {
            self.expect_type(value.ty)?;
            if let Some(producer) = value.producer {
                let op = self.expect_operation(producer)?;
                if !op.results.contains(&Value::new(index)) {
                    return Err(IrError::Verify(format!(
                        "{producer:?} does not own produced value {:?}",
                        Value::new(index)
                    )));
                }
            }
            for user in &value.users {
                let op = self.expect_operation(*user)?;
                if !op.operands.contains(&Value::new(index)) {
                    return Err(IrError::Verify(format!(
                        "{user:?} does not use value {:?}",
                        Value::new(index)
                    )));
                }
            }
        }
        for (index, op) in self.operations.iter().enumerate() {
            if op.erased {
                continue;
            }
            self.expect_type(op.ty)?;
            for operand in &op.operands {
                let value = self.expect_value(*operand)?;
                if !value.users.contains(&Operation::new(index)) {
                    return Err(IrError::Verify(format!(
                        "{operand:?} does not list {:?} as a user",
                        Operation::new(index)
                    )));
                }
            }
            for result in &op.results {
                let value = self.expect_value(*result)?;
                if value.producer != Some(Operation::new(index)) {
                    return Err(IrError::Verify(format!(
                        "{result:?} does not point back to {:?}",
                        Operation::new(index)
                    )));
                }
            }
        }
        self.topological_operations()?;
        Ok(())
    }
}
