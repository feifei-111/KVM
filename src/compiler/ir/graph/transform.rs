use super::Graph;
use crate::compiler::ir::{IrError, Operation, Value};

impl Graph {
    pub fn replace_value(&mut self, old: Value, new: Value) -> Result<(), IrError> {
        self.expect_value(old)?;
        self.expect_value(new)?;
        if old == new {
            return Ok(());
        }

        let old_users = self.values[old.index()].users.clone();
        for op in &old_users {
            let data = &mut self.operations[op.index()];
            for operand in &mut data.operands {
                if *operand == old {
                    *operand = new;
                }
            }
        }
        self.values[old.index()].users.clear();
        for op in old_users {
            if !self.values[new.index()].users.contains(&op) {
                self.values[new.index()].users.push(op);
            }
        }
        Ok(())
    }

    pub fn replace_operation(
        &mut self,
        old: Operation,
        replacement: Operation,
        result_mapping: impl IntoIterator<Item = (Value, Value)>,
    ) -> Result<(), IrError> {
        self.expect_operation(old)?;
        self.expect_operation(replacement)?;
        for (old_value, new_value) in result_mapping {
            let producer = self.producer(old_value)?;
            if producer != Some(old) {
                return Err(IrError::InvalidReplacement(format!(
                    "{old_value:?} is not produced by {old:?}"
                )));
            }
            self.replace_value(old_value, new_value)?;
        }
        self.operations[old.index()].erased = true;
        Ok(())
    }

    pub fn erase_operation(&mut self, op: Operation) -> Result<(), IrError> {
        let data = self.expect_operation(op)?;
        if data
            .results
            .iter()
            .any(|result| !self.values[result.index()].users.is_empty())
        {
            return Err(IrError::InvalidReplacement(format!(
                "{op:?} still has live results"
            )));
        }
        let operands = data.operands.clone();
        for operand in operands {
            self.values[operand.index()].users.retain(|user| *user != op);
        }
        self.operations[op.index()].erased = true;
        Ok(())
    }

    pub fn erase_dead_operations(&mut self) -> Result<Vec<Operation>, IrError> {
        let mut erased = Vec::new();
        for index in (0..self.operations.len()).rev() {
            let op = Operation::new(index);
            if self.operations[index].erased {
                continue;
            }
            if self.operations[index]
                .results
                .iter()
                .all(|result| self.values[result.index()].users.is_empty())
            {
                self.erase_operation(op)?;
                erased.push(op);
            }
        }
        erased.reverse();
        Ok(erased)
    }
}
