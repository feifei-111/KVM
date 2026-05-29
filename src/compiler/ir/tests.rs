use super::*;
use crate::compiler::dialects::kernel::{DType, KernelBuilder, Layout, TensorType};

fn tensor_type() -> TypeExpr {
    TypeExpr::new("kernel", "tensor")
        .with_param("dtype", Attr::Symbol("f16".to_string()))
        .with_param("shape", Attr::List(vec![Attr::Int(128), Attr::Int(128)]))
        .with_param("layout", Attr::Symbol("row_major".to_string()))
}

fn matmul_type() -> TypeExpr {
    TypeExpr::new("kernel", "matmul")
        .with_param("accum", Attr::Symbol("f32".to_string()))
}

fn debug_attrs(name: &str) -> AttrMap {
    AttrMap::from([("debug.name".to_string(), Attr::String(name.to_string()))])
}

#[test]
fn constructs_typed_value_operation_graph() {
    let mut graph = Graph::new();
    let tensor = graph.define_type(tensor_type());
    let matmul_ty = graph.define_type(matmul_type());

    let lhs = graph.add_named_input(Some("lhs"), tensor, AttrMap::new()).unwrap();
    let rhs = graph.add_named_input(Some("rhs"), tensor, AttrMap::new()).unwrap();
    let matmul = graph
        .add_named_operation(
            Some("qk"),
            matmul_ty,
            [lhs, rhs],
            [tensor],
            debug_attrs("qk"),
        )
        .unwrap();
    let out = graph.results(matmul).unwrap()[0];

    assert_eq!(graph.type_of_value(lhs).unwrap(), tensor);
    assert_eq!(graph.type_of_operation(matmul).unwrap(), matmul_ty);
    assert_eq!(graph.producer(out).unwrap(), Some(matmul));
    assert_eq!(graph.users(lhs).unwrap(), vec![matmul]);
    assert_eq!(graph.operands(matmul).unwrap(), &[lhs, rhs]);
    assert_eq!(
        graph.get_operation_attr(matmul, "debug.name").unwrap(),
        Some(&Attr::String("qk".to_string()))
    );
    graph.verify().unwrap();
}

#[test]
fn kernel_builder_creates_public_high_level_dialect_ops() {
    let mut graph = Graph::new();
    let mut kernel = KernelBuilder::new(&mut graph);
    let tensor = kernel.tensor_type(TensorType::new(
        DType::F16,
        [128usize, 128usize],
        Layout::RowMajor,
    ));
    let lhs = kernel.input("lhs", tensor).unwrap();
    let rhs = kernel.input("rhs", tensor).unwrap();
    let out = kernel.matmul(lhs, rhs, tensor, DType::F32).unwrap();

    assert_eq!(
        graph.type_expr(graph.type_of_value(out).unwrap()).unwrap().dialect(),
        "kernel"
    );
    let producer = graph.producer(out).unwrap().unwrap();
    let op_ty = graph.type_expr(graph.type_of_operation(producer).unwrap()).unwrap();
    assert_eq!(op_ty.dialect(), "kernel");
    assert_eq!(op_ty.kind(), "matmul");
    assert_eq!(
        graph.to_text().unwrap(),
        "lhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>\n\
rhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>\n\n\
v2 = kernel.matmul<accum=f32>(lhs, rhs)\n    -> kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>"
    );
    graph.verify().unwrap();
}

#[test]
fn topological_walk_visits_producers_before_users() {
    let mut graph = Graph::new();
    let tensor = graph.define_type(tensor_type());
    let matmul_ty = graph.define_type(matmul_type());
    let lhs = graph.add_input(tensor).unwrap();
    let rhs = graph.add_input(tensor).unwrap();
    let first =
        graph.add_operation(matmul_ty, [lhs, rhs], [tensor], AttrMap::new()).unwrap();
    let first_out = graph.results(first).unwrap()[0];
    let second = graph
        .add_operation(matmul_ty, [first_out, rhs], [tensor], AttrMap::new())
        .unwrap();

    assert_eq!(graph.topological_operations().unwrap(), vec![first, second]);

    let mut walked = Vec::new();
    graph
        .walk_topological(|op| {
            walked.push(op);
            Ok(())
        })
        .unwrap();
    assert_eq!(walked, vec![first, second]);
}

#[test]
fn replace_value_rewrites_users_without_touching_types() {
    let mut graph = Graph::new();
    let tensor = graph.define_type(tensor_type());
    let matmul_ty = graph.define_type(matmul_type());
    let lhs = graph.add_input(tensor).unwrap();
    let rhs = graph.add_input(tensor).unwrap();
    let replacement = graph.add_input(tensor).unwrap();
    let matmul =
        graph.add_operation(matmul_ty, [lhs, rhs], [tensor], AttrMap::new()).unwrap();

    graph.replace_value(lhs, replacement).unwrap();

    assert_eq!(graph.operands(matmul).unwrap(), &[replacement, rhs]);
    assert!(graph.users(lhs).unwrap().is_empty());
    assert_eq!(graph.users(replacement).unwrap(), vec![matmul]);
    assert_eq!(graph.type_of_value(replacement).unwrap(), tensor);
    graph.verify().unwrap();
}

#[test]
fn text_round_trip_uses_clean_kvm_format() {
    let text = r#"
lhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>
rhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>

out = kernel.matmul<accum=f32>(lhs, rhs)
    -> kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>
    { debug.name = "qk_matmul" }
"#;

    let graph = Graph::parse_text(text).unwrap();
    graph.verify().unwrap();
    let printed = graph.to_text().unwrap();

    assert!(
        printed.contains(
            "lhs : kernel.tensor<dtype=f16, layout=row_major, shape=[128,128]>"
        )
    );
    assert!(printed.contains("out = kernel.matmul<accum=f32>(lhs, rhs)"));
    assert!(printed.contains("{ debug.name = \"qk_matmul\" }"));
    assert!(!printed.contains('!'));
    assert!(!printed.contains('%'));
    assert!(!printed.contains('@'));
    assert!(!printed.contains("attrs"));

    let reparsed = Graph::parse_text(&printed).unwrap();
    assert_eq!(reparsed.to_text().unwrap(), printed);
}

#[test]
fn render_text_and_dot_expose_graph_shape() {
    let mut graph = Graph::new();
    let tensor = graph.define_type(tensor_type());
    let matmul_ty = graph.define_type(matmul_type());
    let lhs = graph.add_named_input(Some("lhs"), tensor, AttrMap::new()).unwrap();
    let rhs = graph.add_named_input(Some("rhs"), tensor, AttrMap::new()).unwrap();
    let op = graph
        .add_named_operation(
            Some("qk"),
            matmul_ty,
            [lhs, rhs],
            [tensor],
            AttrMap::new(),
        )
        .unwrap();
    let out = graph.results(op).unwrap()[0];
    graph.set_value_name(out, "out").unwrap();

    let text = graph
        .render(
            RenderFormat::Text,
            RenderOptions { show_types: true, show_attrs: false },
        )
        .unwrap();
    assert!(text.contains("qk [kernel.matmul<accum=f32>]"));
    assert!(text.contains("<- lhs"));
    assert!(text.contains("-> out"));

    let dot = graph
        .render(
            RenderFormat::Dot,
            RenderOptions { show_types: true, show_attrs: false },
        )
        .unwrap();
    assert!(dot.starts_with("digraph ir"));
    assert!(dot.contains("kernel.matmul<accum=f32>"));
    assert!(dot.contains("value"));
}

#[test]
fn type_is_required_and_attr_is_optional() {
    let mut graph = Graph::new();
    let tensor = graph.define_type(tensor_type());
    let value = graph.add_input(tensor).unwrap();

    assert!(graph.attrs_of_value(value).unwrap().is_empty());
    graph.set_value_attr(value, "analysis.rank", Attr::Int(2)).unwrap();
    assert_eq!(
        graph.get_value_attr(value, "analysis.rank").unwrap(),
        Some(&Attr::Int(2))
    );
    assert_eq!(
        graph.remove_value_attr(value, "analysis.rank").unwrap(),
        Some(Attr::Int(2))
    );
    assert_eq!(graph.type_of_value(value).unwrap(), tensor);
    graph.verify().unwrap();

    assert!(matches!(graph.add_input(Type::new(999)), Err(IrError::MissingType(_))));
}
