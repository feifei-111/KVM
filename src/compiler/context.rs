use std::collections::BTreeMap;

use crate::compiler::ir::base::{
    Attribute, AttributeId, IrType, Operator, OperatorId, Type, Variable, VariableId,
};

#[derive(Clone, Debug)]
pub struct Context<T: IrType = Type> {
    next_attribute_id: usize,
    attributes: BTreeMap<AttributeId, Attribute>,
    next_variable_id: usize,
    variables: BTreeMap<VariableId, Variable<T>>,
    next_operator_id: usize,
    operators: BTreeMap<OperatorId, Operator>,
}

impl<T: IrType> Context<T> {
    pub fn new() -> Self {
        Self {
            next_attribute_id: 0,
            attributes: BTreeMap::new(),
            next_variable_id: 0,
            variables: BTreeMap::new(),
            next_operator_id: 0,
            operators: BTreeMap::new(),
        }
    }

    pub fn add_attribute(&mut self, attr: impl Into<Attribute>) -> AttributeId {
        let id = AttributeId(self.next_attribute_id);
        self.next_attribute_id += 1;
        self.attributes.insert(id, attr.into());
        id
    }

    pub fn attribute(&self, id: AttributeId) -> Option<&Attribute> {
        self.attributes.get(&id)
    }

    pub fn attribute_mut(&mut self, id: AttributeId) -> Option<&mut Attribute> {
        self.attributes.get_mut(&id)
    }

    pub fn add_variable(&mut self, variable: Variable<T>) -> VariableId {
        let id = VariableId(self.next_variable_id);
        self.next_variable_id += 1;
        self.variables.insert(id, variable);
        id
    }

    pub fn variable(&self, id: VariableId) -> Option<&Variable<T>> {
        self.variables.get(&id)
    }

    pub fn variable_mut(&mut self, id: VariableId) -> Option<&mut Variable<T>> {
        self.variables.get_mut(&id)
    }

    pub fn add_operator(
        &mut self,
        operator: Operator,
    ) -> Result<OperatorId, ContextError> {
        for input in &operator.inputs {
            if !self.variables.contains_key(input) {
                return Err(ContextError::UnknownVariable(*input));
            }
        }

        for output in &operator.outputs {
            let variable = self
                .variables
                .get(output)
                .ok_or(ContextError::UnknownVariable(*output))?;

            if let Some(existing) = variable.def {
                return Err(ContextError::VariableAlreadyDefined {
                    variable: *output,
                    existing,
                });
            }
        }

        let id = OperatorId(self.next_operator_id);
        self.next_operator_id += 1;

        for input in &operator.inputs {
            self.variables.get_mut(input).unwrap().users.insert(id);
        }

        for output in &operator.outputs {
            self.variables.get_mut(output).unwrap().def = Some(id);
        }

        self.operators.insert(id, operator);
        Ok(id)
    }

    pub fn operator(&self, id: OperatorId) -> Option<&Operator> {
        self.operators.get(&id)
    }

    pub fn operator_mut(&mut self, id: OperatorId) -> Option<&mut Operator> {
        self.operators.get_mut(&id)
    }

    pub fn remove_operator(
        &mut self,
        id: OperatorId,
    ) -> Result<Operator, ContextError> {
        let operator =
            self.operators.remove(&id).ok_or(ContextError::UnknownOperator(id))?;

        for input in &operator.inputs {
            if let Some(variable) = self.variables.get_mut(input) {
                variable.users.remove(&id);
            }
        }

        for output in &operator.outputs {
            if let Some(variable) = self.variables.get_mut(output) {
                if variable.def == Some(id) {
                    variable.def = None;
                }
            }
        }

        Ok(operator)
    }

    pub fn remove_variable(
        &mut self,
        id: VariableId,
    ) -> Result<Variable<T>, ContextError> {
        let variable =
            self.variables.get(&id).ok_or(ContextError::UnknownVariable(id))?;

        if let Some(def) = variable.def {
            return Err(ContextError::VariableStillDefined { variable: id, def });
        }

        if !variable.users.is_empty() {
            return Err(ContextError::VariableStillUsed {
                variable: id,
                users: variable.users.iter().copied().collect(),
            });
        }

        Ok(self.variables.remove(&id).unwrap())
    }
}

impl<T: IrType> Default for Context<T> {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ContextError {
    UnknownVariable(VariableId),
    UnknownOperator(OperatorId),
    VariableAlreadyDefined { variable: VariableId, existing: OperatorId },
    VariableStillDefined { variable: VariableId, def: OperatorId },
    VariableStillUsed { variable: VariableId, users: Vec<OperatorId> },
}
