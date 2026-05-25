use super::attribute::{AttributeId, AttributeMap};
use super::variable::VariableId;

#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct OperatorId(pub usize);

#[derive(Clone, Debug, PartialEq)]
pub struct Operator {
    pub name: String,
    pub inputs: Vec<VariableId>,
    pub outputs: Vec<VariableId>,
    pub attrs: AttributeMap,
}

impl Operator {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            inputs: Vec::new(),
            outputs: Vec::new(),
            attrs: AttributeMap::new(),
        }
    }

    pub fn with_input(mut self, variable: VariableId) -> Self {
        self.inputs.push(variable);
        self
    }

    pub fn with_output(mut self, variable: VariableId) -> Self {
        self.outputs.push(variable);
        self
    }

    pub fn with_attr(mut self, name: impl Into<String>, value: AttributeId) -> Self {
        self.attrs.insert(name.into(), value);
        self
    }
}
