use crate::compiler::dialect::{Dialect, DialectRegistry, OperatorObj};
use crate::compiler::ir::{
    AttrName, Attribute, OpName, Operation, OperationIndex, Type, TypeName, Value,
    ValueIndex,
};
use crate::compiler::store::Store;

pub type ContextResult<T> = Result<T, ContextError>;

#[derive(Default)]
pub struct Context {
    values: Store<Value>,
    operations: Store<Operation>,
    dialects: DialectRegistry,
}

impl Context {
    pub fn new() -> Self {
        return Self::default();
    }

    pub fn register_dialect(&mut self, dialect: Box<dyn Dialect>) -> ContextResult<()> {
        return self.dialects.register(dialect).map_err(ContextError::Dialect);
    }

    pub fn dialect(&self, name: &str) -> Option<&dyn Dialect> {
        return self.dialects.get(name);
    }

    pub fn operator(&self, name: &OpName) -> Option<&dyn OperatorObj> {
        return self.dialects.operator(name);
    }

    pub fn ty(&self, name: &TypeName) -> Option<&dyn Type> {
        return self.dialects.ty(name);
    }

    pub fn attribute(&self, name: &AttrName) -> Option<&dyn Attribute> {
        return self.dialects.attribute(name);
    }

    pub fn add_value(&mut self, value: Value) -> ContextResult<ValueIndex> {
        if self.dialects.ty(&value.ty).is_none() {
            return Err(ContextError::UnknownTypeName(value.ty));
        }
        return Ok(self.values.insert(value));
    }

    pub fn value(&self, index: ValueIndex) -> Option<&Value> {
        return self.values.get(index);
    }

    pub fn value_mut(&mut self, index: ValueIndex) -> Option<&mut Value> {
        return self.values.get_mut(index);
    }

    pub fn add_operation(
        &mut self,
        operation: Operation,
    ) -> ContextResult<OperationIndex> {
        if self.dialects.operator(&operation.name).is_none() {
            return Err(ContextError::UnknownOperationName(operation.name));
        }

        for (_, attr) in &operation.attrs {
            if self.dialects.attribute(&attr.name()).is_none() {
                return Err(ContextError::UnknownAttributeName(attr.name()));
            }
        }

        for operand in &operation.operands {
            if !self.values.contains(*operand) {
                return Err(ContextError::UnknownValue(*operand));
            }
        }

        for result in &operation.results {
            let value =
                self.values.get(*result).ok_or(ContextError::UnknownValue(*result))?;

            if let Some(existing) = value.def {
                return Err(ContextError::ValueAlreadyDefined {
                    value: *result,
                    existing,
                });
            }
        }

        let operands = operation.operands.clone();
        let results = operation.results.clone();
        let index = self.operations.insert(operation);

        for operand in operands {
            self.values.get_mut(operand).unwrap().users.insert(index);
        }

        for result in results {
            self.values.get_mut(result).unwrap().def = Some(index);
        }

        return Ok(index);
    }

    pub fn operation(&self, index: OperationIndex) -> Option<&Operation> {
        return self.operations.get(index);
    }

    pub fn operation_mut(&mut self, index: OperationIndex) -> Option<&mut Operation> {
        return self.operations.get_mut(index);
    }

    pub fn remove_operation(
        &mut self,
        index: OperationIndex,
    ) -> ContextResult<Operation> {
        let operation = self
            .operations
            .remove(index)
            .ok_or(ContextError::UnknownOperation(index))?;

        for operand in &operation.operands {
            if let Some(value) = self.values.get_mut(*operand) {
                value.users.remove(&index);
            }
        }

        for result in &operation.results {
            if let Some(value) = self.values.get_mut(*result) {
                if value.def == Some(index) {
                    value.def = None;
                }
            }
        }

        return Ok(operation);
    }

    pub fn remove_value(&mut self, index: ValueIndex) -> ContextResult<Value> {
        let value = self.values.get(index).ok_or(ContextError::UnknownValue(index))?;

        if let Some(def) = value.def {
            return Err(ContextError::ValueStillDefined { value: index, def });
        }

        if !value.users.is_empty() {
            return Err(ContextError::ValueStillUsed {
                value: index,
                users: value.users.iter().copied().collect(),
            });
        }

        return Ok(self.values.remove(index).unwrap());
    }

    pub fn verify_operation(&self, index: OperationIndex) -> ContextResult<()> {
        let operation =
            self.operations.get(index).ok_or(ContextError::UnknownOperation(index))?;
        let operator = self.dialects.operator(&operation.name).ok_or_else(|| {
            ContextError::UnknownOperationName(operation.name.clone())
        })?;

        return operator.verify(self, index).map_err(ContextError::Dialect);
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ContextError {
    Dialect(String),
    UnknownAttributeName(AttrName),
    UnknownTypeName(TypeName),
    UnknownOperationName(OpName),
    UnknownValue(ValueIndex),
    UnknownOperation(OperationIndex),
    ValueAlreadyDefined { value: ValueIndex, existing: OperationIndex },
    ValueStillDefined { value: ValueIndex, def: OperationIndex },
    ValueStillUsed { value: ValueIndex, users: Vec<OperationIndex> },
}
