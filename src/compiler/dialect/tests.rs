use super::Dialect;
use super::hir::AddOp;
use crate::compiler::context::{Context, ContextError};
use crate::compiler::ir::{AttributeMap, Operation, Operator, Type, Value};
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
        return "hir";
    }

    fn kind(&self) -> &str {
        return "tensor";
    }

    fn identity(&self) -> String {
        return format!("hir::tensor<{:?},{}>", self.shape, self.dtype);
    }

    fn as_any(&self) -> &dyn Any {
        return self;
    }
}

struct TestDialect;

impl Dialect for TestDialect {
    fn name(&self) -> &str {
        return "hir";
    }

    fn operators(&self) -> Vec<Box<dyn Operator>> {
        return vec![Box::new(AddOp)];
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

    let ty = context.type_index("hir::tensor<[2, 2],f16>").unwrap();
    let lhs = context.add_value(Value::new("lhs", ty, AttributeMap::new())).unwrap();
    let rhs = context.add_value(Value::new("rhs", ty, AttributeMap::new())).unwrap();
    let add = context.operator_index(AddOp::DIALECT, AddOp::KIND).unwrap();

    let op =
        context.build_operation(add, vec![lhs, rhs], Vec::new(), AttributeMap::new());
    assert!(op.is_ok());

    let operation = context.operation(op.unwrap()).unwrap();
    let add_operator = context
        .operator(operation.operator)
        .unwrap()
        .as_any()
        .downcast_ref::<AddOp>()
        .unwrap();
    let result = AddOp::result(operation).unwrap();

    assert_eq!(add_operator.kind(), AddOp::KIND);
    assert_eq!(AddOp::lhs(operation), Some(lhs));
    assert_eq!(AddOp::rhs(operation), Some(rhs));
    assert_eq!(context.value(result).unwrap().ty, ty);
}

#[test]
fn rejects_add_with_different_operand_types() {
    let mut context = Context::new();
    context.register_dialect(Box::new(TestDialect)).unwrap();

    let ty_a = context.type_index("hir::tensor<[2, 2],f16>").unwrap();
    let ty_b = context.type_index("hir::tensor<[4, 4],f16>").unwrap();
    let lhs = context.add_value(Value::new("lhs", ty_a, AttributeMap::new())).unwrap();
    let rhs = context.add_value(Value::new("rhs", ty_b, AttributeMap::new())).unwrap();
    let result =
        context.add_value(Value::new("result", ty_a, AttributeMap::new())).unwrap();
    let add = context.operator_index(AddOp::DIALECT, AddOp::KIND).unwrap();

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
