use std::collections::BTreeMap;
use std::marker::PhantomData;

use crate::compiler::context::Context;
use crate::compiler::ir::{
    AttrName, Attribute, OpName, OperationIndex, Type, TypeName,
};

pub trait Op {
    fn name() -> OpName
    where
        Self: Sized;

    fn from_operation(operation: OperationIndex) -> Self
    where
        Self: Sized;

    fn operation(&self) -> OperationIndex;
}

pub trait Signature: std::fmt::Debug {
    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String>;
}

pub struct Operator<Sig: Signature> {
    name: OpName,
    signature: Sig,
}

impl<Sig: Signature> Operator<Sig> {
    pub fn new(name: OpName, signature: Sig) -> Self {
        return Self { name, signature };
    }
}

pub trait OperatorObj {
    fn name(&self) -> &OpName;
    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String>;
}

impl<Sig: Signature> OperatorObj for Operator<Sig> {
    fn name(&self) -> &OpName {
        return &self.name;
    }

    fn verify(
        &self,
        context: &Context,
        operation: OperationIndex,
    ) -> Result<(), String> {
        return self.signature.verify(context, operation);
    }
}

#[derive(Debug)]
pub struct EmptySignature {
    _marker: PhantomData<()>,
}

impl EmptySignature {
    pub fn new() -> Self {
        return Self { _marker: PhantomData };
    }
}

impl Signature for EmptySignature {
    fn verify(
        &self,
        _context: &Context,
        _operation: OperationIndex,
    ) -> Result<(), String> {
        return Ok(());
    }
}

pub trait Dialect {
    fn name(&self) -> &str;

    fn operators(&self) -> Vec<Box<dyn OperatorObj>> {
        return Vec::new();
    }

    fn types(&self) -> Vec<Box<dyn Type>> {
        return Vec::new();
    }

    fn attributes(&self) -> Vec<Box<dyn Attribute>> {
        return Vec::new();
    }
}

#[derive(Default)]
pub struct DialectRegistry {
    dialects: BTreeMap<String, Box<dyn Dialect>>,
    operators: BTreeMap<OpName, Box<dyn OperatorObj>>,
    types: BTreeMap<TypeName, Box<dyn Type>>,
    attributes: BTreeMap<AttrName, Box<dyn Attribute>>,
}

impl DialectRegistry {
    pub fn new() -> Self {
        return Self::default();
    }

    pub fn contains(&self, name: &str) -> bool {
        return self.dialects.contains_key(name);
    }

    pub fn get(&self, name: &str) -> Option<&dyn Dialect> {
        return self.dialects.get(name).map(Box::as_ref);
    }

    pub fn register(&mut self, dialect: Box<dyn Dialect>) -> Result<(), String> {
        let dialect_name = dialect.name().to_string();
        if self.dialects.contains_key(&dialect_name) {
            return Err(format!("dialect `{dialect_name}` is already registered"));
        }

        for operator in dialect.operators() {
            if operator.name().dialect != dialect_name {
                return Err(format!(
                    "operator `{}::{}` does not belong to dialect `{}`",
                    operator.name().dialect,
                    operator.name().name,
                    dialect_name
                ));
            }
            self.operators.insert(operator.name().clone(), operator);
        }

        for ty in dialect.types() {
            let name = ty.name();
            if name.dialect != dialect_name {
                return Err(format!(
                    "type `{}::{}` does not belong to dialect `{}`",
                    name.dialect, name.name, dialect_name
                ));
            }
            self.types.insert(name, ty);
        }

        for attr in dialect.attributes() {
            let name = attr.name();
            if name.dialect != dialect_name {
                return Err(format!(
                    "attribute `{}::{}` does not belong to dialect `{}`",
                    name.dialect, name.name, dialect_name
                ));
            }
            self.attributes.insert(name, attr);
        }

        self.dialects.insert(dialect_name, dialect);
        return Ok(());
    }

    pub fn operator(&self, name: &OpName) -> Option<&dyn OperatorObj> {
        return self.operators.get(name).map(Box::as_ref);
    }

    pub fn ty(&self, name: &TypeName) -> Option<&dyn Type> {
        return self.types.get(name).map(Box::as_ref);
    }

    pub fn attribute(&self, name: &AttrName) -> Option<&dyn Attribute> {
        return self.attributes.get(name).map(Box::as_ref);
    }
}
