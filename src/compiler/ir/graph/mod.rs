mod format;
mod render;
mod transform;
mod traversal;
mod verify;

use super::{Attr, AttrMap, IrError, Operation, Type, TypeExpr, Value};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ValueView<'a> {
    pub value: Value,
    pub ty: Type,
    pub attrs: &'a AttrMap,
    pub producer: Option<Operation>,
    pub name: Option<&'a str>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OperationView<'a> {
    pub operation: Operation,
    pub ty: Type,
    pub operands: &'a [Value],
    pub results: &'a [Value],
    pub attrs: &'a AttrMap,
    pub name: Option<&'a str>,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct RenderOptions {
    pub show_types: bool,
    pub show_attrs: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum RenderFormat {
    Text,
    Dot,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Graph {
    types: Vec<TypeExpr>,
    values: Vec<ValueData>,
    operations: Vec<OperationData>,
}

impl Graph {
    pub fn new() -> Self {
        Self { types: Vec::new(), values: Vec::new(), operations: Vec::new() }
    }

    pub fn define_type(&mut self, ty: TypeExpr) -> Type {
        self.types.push(ty);
        Type::new(self.types.len() - 1)
    }

    pub fn type_expr(&self, ty: Type) -> Result<&TypeExpr, IrError> {
        self.types.get(ty.index()).ok_or(IrError::MissingType(ty))
    }

    pub fn add_input(&mut self, ty: Type) -> Result<Value, IrError> {
        self.add_named_input(None::<String>, ty, AttrMap::new())
    }

    pub fn add_named_input(
        &mut self,
        name: Option<impl Into<String>>,
        ty: Type,
        attrs: AttrMap,
    ) -> Result<Value, IrError> {
        self.expect_type(ty)?;
        Ok(self.push_value(name.map(Into::into), ty, attrs, None))
    }

    pub fn add_constant(&mut self, ty: Type, attrs: AttrMap) -> Result<Value, IrError> {
        self.expect_type(ty)?;
        Ok(self.push_value(None, ty, attrs, None))
    }

    pub fn add_operation(
        &mut self,
        ty: Type,
        operands: impl IntoIterator<Item = Value>,
        result_types: impl IntoIterator<Item = Type>,
        attrs: AttrMap,
    ) -> Result<Operation, IrError> {
        self.add_named_operation(None::<String>, ty, operands, result_types, attrs)
    }

    pub fn add_named_operation(
        &mut self,
        name: Option<impl Into<String>>,
        ty: Type,
        operands: impl IntoIterator<Item = Value>,
        result_types: impl IntoIterator<Item = Type>,
        attrs: AttrMap,
    ) -> Result<Operation, IrError> {
        self.expect_type(ty)?;
        let operands: Vec<_> = operands.into_iter().collect();
        for operand in &operands {
            self.expect_value(*operand)?;
        }
        let result_types: Vec<_> = result_types.into_iter().collect();
        for result_ty in &result_types {
            self.expect_type(*result_ty)?;
        }

        let op = Operation::new(self.operations.len());
        let mut results = Vec::with_capacity(result_types.len());
        for result_ty in result_types {
            results.push(self.push_value(None, result_ty, AttrMap::new(), Some(op)));
        }
        self.operations.push(OperationData {
            name: name.map(Into::into),
            ty,
            operands: operands.clone(),
            results,
            attrs,
            erased: false,
        });
        for operand in operands {
            self.values[operand.index()].users.push(op);
        }
        Ok(op)
    }

    pub fn value(&self, value: Value) -> Result<ValueView<'_>, IrError> {
        let data = self.expect_value(value)?;
        Ok(ValueView {
            value,
            ty: data.ty,
            attrs: &data.attrs,
            producer: data.producer,
            name: data.name.as_deref(),
        })
    }

    pub fn operation(&self, op: Operation) -> Result<OperationView<'_>, IrError> {
        let data = self.expect_operation(op)?;
        Ok(OperationView {
            operation: op,
            ty: data.ty,
            operands: &data.operands,
            results: &data.results,
            attrs: &data.attrs,
            name: data.name.as_deref(),
        })
    }

    pub fn type_of_value(&self, value: Value) -> Result<Type, IrError> {
        Ok(self.expect_value(value)?.ty)
    }

    pub fn type_of_operation(&self, op: Operation) -> Result<Type, IrError> {
        Ok(self.expect_operation(op)?.ty)
    }

    pub fn attrs_of_value(&self, value: Value) -> Result<&AttrMap, IrError> {
        Ok(&self.expect_value(value)?.attrs)
    }

    pub fn attrs_of_operation(&self, op: Operation) -> Result<&AttrMap, IrError> {
        Ok(&self.expect_operation(op)?.attrs)
    }

    pub fn set_value_attr(
        &mut self,
        value: Value,
        key: impl Into<String>,
        attr: Attr,
    ) -> Result<(), IrError> {
        self.expect_value(value)?;
        self.values[value.index()].attrs.insert(key.into(), attr);
        Ok(())
    }

    pub fn set_operation_attr(
        &mut self,
        op: Operation,
        key: impl Into<String>,
        attr: Attr,
    ) -> Result<(), IrError> {
        self.expect_operation(op)?;
        self.operations[op.index()].attrs.insert(key.into(), attr);
        Ok(())
    }

    pub fn get_value_attr(
        &self,
        value: Value,
        key: &str,
    ) -> Result<Option<&Attr>, IrError> {
        Ok(self.expect_value(value)?.attrs.get(key))
    }

    pub fn get_operation_attr(
        &self,
        op: Operation,
        key: &str,
    ) -> Result<Option<&Attr>, IrError> {
        Ok(self.expect_operation(op)?.attrs.get(key))
    }

    pub fn remove_value_attr(
        &mut self,
        value: Value,
        key: &str,
    ) -> Result<Option<Attr>, IrError> {
        self.expect_value(value)?;
        Ok(self.values[value.index()].attrs.remove(key))
    }

    pub fn remove_operation_attr(
        &mut self,
        op: Operation,
        key: &str,
    ) -> Result<Option<Attr>, IrError> {
        self.expect_operation(op)?;
        Ok(self.operations[op.index()].attrs.remove(key))
    }

    pub fn set_value_name(
        &mut self,
        value: Value,
        name: impl Into<String>,
    ) -> Result<(), IrError> {
        self.expect_value(value)?;
        self.values[value.index()].name = Some(name.into());
        Ok(())
    }

    pub fn producer(&self, value: Value) -> Result<Option<Operation>, IrError> {
        Ok(self.expect_value(value)?.producer)
    }

    pub fn users(&self, value: Value) -> Result<Vec<Operation>, IrError> {
        Ok(self.expect_value(value)?.users.clone())
    }

    pub fn operands(&self, op: Operation) -> Result<&[Value], IrError> {
        Ok(&self.expect_operation(op)?.operands)
    }

    pub fn results(&self, op: Operation) -> Result<&[Value], IrError> {
        Ok(&self.expect_operation(op)?.results)
    }

    fn push_value(
        &mut self,
        name: Option<String>,
        ty: Type,
        attrs: AttrMap,
        producer: Option<Operation>,
    ) -> Value {
        let value = Value::new(self.values.len());
        self.values.push(ValueData { name, ty, attrs, producer, users: Vec::new() });
        value
    }

    fn expect_type(&self, ty: Type) -> Result<&TypeExpr, IrError> {
        self.types.get(ty.index()).ok_or(IrError::MissingType(ty))
    }

    fn expect_value(&self, value: Value) -> Result<&ValueData, IrError> {
        self.values.get(value.index()).ok_or(IrError::MissingValue(value))
    }

    fn expect_operation(&self, op: Operation) -> Result<&OperationData, IrError> {
        match self.operations.get(op.index()) {
            Some(data) if !data.erased => Ok(data),
            _ => Err(IrError::MissingOperation(op)),
        }
    }

    fn value_names(&self) -> Vec<String> {
        self.values
            .iter()
            .enumerate()
            .map(|(index, value)| {
                value.name.clone().unwrap_or_else(|| format!("v{index}"))
            })
            .collect()
    }
}

impl Default for Graph {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct ValueData {
    name: Option<String>,
    ty: Type,
    attrs: AttrMap,
    producer: Option<Operation>,
    users: Vec<Operation>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct OperationData {
    name: Option<String>,
    ty: Type,
    operands: Vec<Value>,
    results: Vec<Value>,
    attrs: AttrMap,
    erased: bool,
}
