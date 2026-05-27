use std::collections::{BTreeMap, BTreeSet};

use crate::compiler::dialect::{Dialect, DialectKey, Operator, OperatorIndex};
use crate::compiler::ir::{
    Attribute, AttributeIndex, AttributeMap, BuiltinAttribute, BuiltinType, Operation,
    OperationIndex, Type, TypeIndex, Value, ValueIndex,
};
use crate::compiler::store::Store;

pub type ContextResult<T> = Result<T, ContextError>;

pub struct Context {
    values: Store<Value>,
    operations: Store<Operation>,
    types: Store<Box<dyn Type>>,
    attributes: Store<Box<dyn Attribute>>,
    operators: Store<Box<dyn Operator>>,
    dialects: BTreeSet<String>,
    type_lookup: BTreeMap<String, TypeIndex>,
    attribute_lookup: BTreeMap<String, AttributeIndex>,
    operator_lookup: BTreeMap<DialectKey, OperatorIndex>,
}

impl Context {
    pub fn new() -> Self {
        let mut context = Self::empty();
        context.register_builtin();
        return context;
    }

    fn empty() -> Self {
        return Self {
            values: Store::new(),
            operations: Store::new(),
            types: Store::new(),
            attributes: Store::new(),
            operators: Store::new(),
            dialects: BTreeSet::new(),
            type_lookup: BTreeMap::new(),
            attribute_lookup: BTreeMap::new(),
            operator_lookup: BTreeMap::new(),
        };
    }

    fn register_builtin(&mut self) {
        self.add_type(Box::new(BuiltinType::Unit)).unwrap();
        self.add_type(Box::new(BuiltinType::Index)).unwrap();
        self.add_attribute(Box::new(BuiltinAttribute::Bool(false))).unwrap();
        self.add_attribute(Box::new(BuiltinAttribute::Int(0))).unwrap();
        self.add_attribute(Box::new(BuiltinAttribute::Float(0.0))).unwrap();
        self.add_attribute(Box::new(BuiltinAttribute::String(String::new()))).unwrap();
    }

    pub fn register_dialect(&mut self, dialect: Box<dyn Dialect>) -> ContextResult<()> {
        let dialect_name = dialect.name().to_string();
        if self.dialects.contains(&dialect_name) {
            return Err(ContextError::DialectAlreadyRegistered(dialect_name));
        }

        for ty in dialect.types() {
            if ty.dialect() != dialect_name {
                return Err(ContextError::ComponentDialectMismatch {
                    expected: dialect_name,
                    actual: ty.dialect().to_string(),
                });
            }
            self.add_type(ty)?;
        }

        for attribute in dialect.attributes() {
            if attribute.dialect() != dialect_name {
                return Err(ContextError::ComponentDialectMismatch {
                    expected: dialect_name,
                    actual: attribute.dialect().to_string(),
                });
            }
            self.add_attribute(attribute)?;
        }

        for operator in dialect.operators() {
            if operator.dialect() != dialect_name {
                return Err(ContextError::ComponentDialectMismatch {
                    expected: dialect_name,
                    actual: operator.dialect().to_string(),
                });
            }
            self.add_operator(operator)?;
        }

        self.dialects.insert(dialect_name);
        return Ok(());
    }

    pub fn type_index(&self, identity: &str) -> Option<TypeIndex> {
        return self.type_lookup.get(identity).copied();
    }

    pub fn type_object(&self, index: TypeIndex) -> Option<&dyn Type> {
        return self.types.get(index).map(Box::as_ref);
    }

    pub fn attribute_index(&self, identity: &str) -> Option<AttributeIndex> {
        return self.attribute_lookup.get(identity).copied();
    }

    pub fn attribute(&self, index: AttributeIndex) -> Option<&dyn Attribute> {
        return self.attributes.get(index).map(Box::as_ref);
    }

    pub fn intern_attribute(
        &mut self,
        attribute: Box<dyn Attribute>,
    ) -> ContextResult<AttributeIndex> {
        return self.add_attribute(attribute);
    }

    pub fn operator_index(&self, dialect: &str, kind: &str) -> Option<OperatorIndex> {
        return self.operator_lookup.get(&DialectKey::new(dialect, kind)).copied();
    }

    pub fn operator(&self, index: OperatorIndex) -> Option<&dyn Operator> {
        return self.operators.get(index).map(Box::as_ref);
    }

    pub fn add_value(&mut self, value: Value) -> ContextResult<ValueIndex> {
        self.require_type(value.ty)?;
        self.require_attributes(&value.attrs)?;
        return Ok(self.values.insert(value));
    }

    pub fn value(&self, index: ValueIndex) -> Option<&Value> {
        return self.values.get(index);
    }

    pub fn value_mut(&mut self, index: ValueIndex) -> Option<&mut Value> {
        return self.values.get_mut(index);
    }

    pub fn build_operation(
        &mut self,
        operator: OperatorIndex,
        operands: Vec<ValueIndex>,
        result_types: Vec<TypeIndex>,
        attrs: AttributeMap,
    ) -> ContextResult<OperationIndex> {
        self.require_operator(operator)?;

        let result_types = self
            .operator(operator)
            .unwrap()
            .infer_results(self, &operands, result_types)
            .map_err(ContextError::Verifier)?;

        let mut results = Vec::with_capacity(result_types.len());
        for (index, ty) in result_types.into_iter().enumerate() {
            let name = format!("result{}", index);
            results.push(self.add_value(Value::new(name, ty, AttributeMap::new()))?);
        }

        let operation = Operation::new(operator, operands, results, attrs);
        let index = self.add_operation(operation)?;
        self.verify_operation(index)?;
        return Ok(index);
    }

    pub fn add_operation(
        &mut self,
        operation: Operation,
    ) -> ContextResult<OperationIndex> {
        self.require_operator(operation.operator)?;
        self.require_attributes(&operation.attrs)?;

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
        let operator = self
            .operators
            .get(operation.operator)
            .ok_or(ContextError::UnknownOperator(operation.operator))?;

        return operator.verify(self, index).map_err(ContextError::Verifier);
    }

    fn add_type(&mut self, ty: Box<dyn Type>) -> ContextResult<TypeIndex> {
        let identity = ty.identity();
        if self.type_lookup.contains_key(&identity) {
            return Err(ContextError::DuplicateType(identity));
        }

        let index = self.types.insert(ty);
        self.type_lookup.insert(identity, index);
        return Ok(index);
    }

    fn add_attribute(
        &mut self,
        attribute: Box<dyn Attribute>,
    ) -> ContextResult<AttributeIndex> {
        let identity = attribute.identity();
        if self.attribute_lookup.contains_key(&identity) {
            return Err(ContextError::DuplicateAttribute(identity));
        }

        let index = self.attributes.insert(attribute);
        self.attribute_lookup.insert(identity, index);
        return Ok(index);
    }

    fn add_operator(
        &mut self,
        operator: Box<dyn Operator>,
    ) -> ContextResult<OperatorIndex> {
        let key = DialectKey::from(operator.as_ref());
        if self.operator_lookup.contains_key(&key) {
            return Err(ContextError::DuplicateOperator(key));
        }

        let index = self.operators.insert(operator);
        self.operator_lookup.insert(key, index);
        return Ok(index);
    }

    fn require_type(&self, index: TypeIndex) -> ContextResult<()> {
        if !self.types.contains(index) {
            return Err(ContextError::UnknownType(index));
        }
        return Ok(());
    }

    fn require_operator(&self, index: OperatorIndex) -> ContextResult<()> {
        if !self.operators.contains(index) {
            return Err(ContextError::UnknownOperator(index));
        }
        return Ok(());
    }

    fn require_attributes(&self, attrs: &AttributeMap) -> ContextResult<()> {
        for attribute in attrs.values() {
            if !self.attributes.contains(*attribute) {
                return Err(ContextError::UnknownAttribute(*attribute));
            }
        }
        return Ok(());
    }
}

impl Default for Context {
    fn default() -> Self {
        return Self::new();
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ContextError {
    ComponentDialectMismatch { expected: String, actual: String },
    DialectAlreadyRegistered(String),
    DuplicateAttribute(String),
    DuplicateOperator(DialectKey),
    DuplicateType(String),
    UnknownAttribute(AttributeIndex),
    UnknownOperator(OperatorIndex),
    UnknownType(TypeIndex),
    UnknownValue(ValueIndex),
    UnknownOperation(OperationIndex),
    ValueAlreadyDefined { value: ValueIndex, existing: OperationIndex },
    ValueStillDefined { value: ValueIndex, def: OperationIndex },
    ValueStillUsed { value: ValueIndex, users: Vec<OperationIndex> },
    Verifier(String),
}
