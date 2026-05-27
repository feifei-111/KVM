use super::{Dialect, Operator, SameOperandsAndResultsType};
use crate::compiler::context::{Context, ContextError};
use crate::compiler::ir::{AttributeMap, Operation, Type, Value};
use std::any::Any;

#[derive(Debug)]
struct TestTensorType {
    shape: Vec<usize>,
    dtype: &'static str,
}

impl TestTensorType {
    fn new(shape: Vec<usize>, dtype: &'static str) -> Self {
        return Self { shape, dtype };
    }
}

impl Type for TestTensorType {
    fn dialect(&self) -> &str {
        return "test";
    }

    fn kind(&self) -> &str {
        return "tensor";
    }

    fn identity(&self) -> String {
        return format!("test::tensor<{:?},{}>", self.shape, self.dtype);
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

struct TestDialect;

impl Dialect for TestDialect {
    fn name(&self) -> &str {
        return "test";
    }

    fn operators(&self) -> Vec<Box<dyn Operator>> {
        return vec![Box::new(SameOperandsAndResultsType::new("test", "add"))];
    }

    fn types(&self) -> Vec<Box<dyn Type>> {
        return vec![
            Box::new(TestTensorType::new(vec![2, 2], "f16")),
            Box::new(TestTensorType::new(vec![4, 4], "f16")),
        ];
    }
}

#[test]
fn infers_add_result_type_from_first_operand() {
    let mut context = Context::new();
    context.register_dialect(Box::new(TestDialect)).unwrap();

    let ty = context.type_index("test::tensor<[2, 2],f16>").unwrap();
    let lhs = context.add_value(Value::new("lhs", ty, AttributeMap::new())).unwrap();
    let rhs = context.add_value(Value::new("rhs", ty, AttributeMap::new())).unwrap();
    let add = context.operator_index("test", "add").unwrap();

    let op =
        context.build_operation(add, vec![lhs, rhs], Vec::new(), AttributeMap::new());
    assert!(op.is_ok());

    let result = context.operation(op.unwrap()).unwrap().results[0];
    assert_eq!(context.value(result).unwrap().ty, ty);
}

#[test]
fn rejects_add_with_different_operand_types() {
    let mut context = Context::new();
    context.register_dialect(Box::new(TestDialect)).unwrap();

    let ty_a = context.type_index("test::tensor<[2, 2],f16>").unwrap();
    let ty_b = context.type_index("test::tensor<[4, 4],f16>").unwrap();
    let lhs = context.add_value(Value::new("lhs", ty_a, AttributeMap::new())).unwrap();
    let rhs = context.add_value(Value::new("rhs", ty_b, AttributeMap::new())).unwrap();
    let result =
        context.add_value(Value::new("result", ty_a, AttributeMap::new())).unwrap();
    let add = context.operator_index("test", "add").unwrap();

    let op = context
        .add_operation(Operation::new(
            add,
            vec![lhs, rhs],
            vec![result],
            AttributeMap::new(),
        ))
        .unwrap();

    let error = context.verify_operation(op).unwrap_err();
    assert!(matches!(error, ContextError::Verifier(_)));
}
