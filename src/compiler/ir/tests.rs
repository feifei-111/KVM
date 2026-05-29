use super::*;
use crate::compiler::dialects::hlo::{DType, HloBuilder, TensorType};
use crate::render::{GraphRenderOptions, render_dot};

fn tensor_type() -> TypeExpr {
    TypeExpr::new("hlo", "tensor")
        .with_field("dtype", Attr::Symbol("f16".to_string()))
        .with_field("shape", Attr::List(vec![Attr::Int(128), Attr::Int(128)]))
}

fn matmul_type() -> TypeExpr {
    TypeExpr::new("hlo", "matmul")
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
fn hlo_builder_creates_public_high_level_dialect_ops() {
    let mut graph = Graph::new();
    let mut hlo = HloBuilder::new(&mut graph);
    let tensor = hlo.tensor_type(TensorType::new(DType::F16, [128usize, 128usize]));
    let lhs = hlo.input("lhs", tensor).unwrap();
    let rhs = hlo.input("rhs", tensor).unwrap();
    let out = hlo.matmul(lhs, rhs, tensor).unwrap();

    assert_eq!(
        graph.type_expr(graph.type_of_value(out).unwrap()).unwrap().dialect(),
        "hlo"
    );
    let producer = graph.producer(out).unwrap().unwrap();
    let op_ty = graph.type_expr(graph.type_of_operation(producer).unwrap()).unwrap();
    assert_eq!(op_ty.dialect(), "hlo");
    assert_eq!(op_ty.kind(), "matmul");
    assert_eq!(
        graph.to_text().unwrap(),
        "%lhs : hlo.tensor<dtype=f16, shape=[128,128]>\n\
%rhs : hlo.tensor<dtype=f16, shape=[128,128]>\n\n\
%v2 = \"hlo.matmul\"(%lhs, %rhs) : (hlo.tensor<dtype=f16, shape=[128,128]>, hlo.tensor<dtype=f16, shape=[128,128]>) -> hlo.tensor<dtype=f16, shape=[128,128]>"
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
graph.dist.config = {"backend":"nccl","ranks":[{"rank":0,"host":"node0","process":0,"device":"cuda:0"},{"rank":1,"host":"node0","process":1,"device":"cuda:1"}]}

%lhs : hlo.tensor<dtype=f16, shape=[128,128]>
%rhs : hlo.tensor<dtype=f16, shape=[128,128]>

%out = "hlo.matmul"(%lhs, %rhs) {"debug.name":"qk_matmul"} : (hlo.tensor<dtype=f16, shape=[128,128]>, hlo.tensor<dtype=f16, shape=[128,128]>) -> hlo.tensor<dtype=f16, shape=[128,128]>
"#;

    let graph = Graph::parse_text(text).unwrap();
    graph.verify().unwrap();
    let printed = graph.to_text().unwrap();

    assert!(printed.contains("%lhs : hlo.tensor<dtype=f16, shape=[128,128]>"));
    assert!(printed.contains("graph.dist.config = {\"backend\":\"nccl\""));
    assert!(printed.contains("%out = \"hlo.matmul\"(%lhs, %rhs)"));
    assert!(printed.contains("{\"debug.name\":\"qk_matmul\"}"));
    assert!(!printed.contains('!'));
    assert!(!printed.contains("attrs"));

    let reparsed = Graph::parse_text(&printed).unwrap();
    assert_eq!(reparsed.to_text().unwrap(), printed);
}

#[test]
fn block_text_marks_call_inputs_and_typed_outputs() {
    let text = r#"
func @main(%tokens: hlo.tensor<dtype=f16, shape=[8,4096]>, %weight: hlo.tensor<dtype=f16, shape=[4096,4096]>) -> hlo.tensor<dtype=f16, shape=[8,4096]> {
  %out = "hlo.matmul"(%tokens, %weight) : (hlo.tensor<dtype=f16, shape=[8,4096]>, hlo.tensor<dtype=f16, shape=[4096,4096]>) -> hlo.tensor<dtype=f16, shape=[8,4096]>
  return %out : hlo.tensor<dtype=f16, shape=[8,4096]>
}
"#;

    let graph = Graph::parse_text(text).unwrap();
    graph.verify().unwrap();

    let block = graph.entry_block().unwrap();
    assert_eq!(graph.block(block).unwrap().name, "main");
    assert_eq!(graph.block_inputs(block).unwrap().len(), 2);
    assert_eq!(graph.block_outputs(block).unwrap().len(), 1);
    assert_eq!(
        graph.value(graph.block_input(block, 0).unwrap()).unwrap().name,
        Some("tokens")
    );

    let printed = graph.to_text().unwrap();
    assert!(printed.contains(
        "func @main(%tokens: hlo.tensor<dtype=f16, shape=[8,4096]>, %weight: hlo.tensor<dtype=f16, shape=[4096,4096]>) -> hlo.tensor<dtype=f16, shape=[8,4096]> {"
    ));
    assert!(printed.contains("return %out : hlo.tensor<dtype=f16, shape=[8,4096]>"));

    let reparsed = Graph::parse_text(&printed).unwrap();
    assert_eq!(reparsed.to_text().unwrap(), printed);
}

#[test]
fn block_input_attrs_round_trip() {
    let text = r#"
func @main(%attn_meta: hlo.attn_meta {"positions":{"start_positions":[12,28]}}, %tokens: hlo.tensor<dtype=f16, shape=[8,4096]>) -> hlo.tensor<dtype=f16, shape=[8,4096]> {
  return %tokens : hlo.tensor<dtype=f16, shape=[8,4096]>
}
"#;

    let graph = Graph::parse_text(text).unwrap();
    graph.verify().unwrap();

    let block = graph.entry_block().unwrap();
    let attn_meta = graph.block_input(block, 0).unwrap();
    assert_eq!(
        graph.get_value_attr(attn_meta, "positions").unwrap(),
        Some(&Attr::Dict(AttrMap::from([(
            "start_positions".to_string(),
            Attr::List(vec![Attr::Int(12), Attr::Int(28)])
        )])))
    );

    let printed = graph.to_text().unwrap();
    assert!(printed.contains(
        "%attn_meta: hlo.attn_meta {\"positions\":{\"start_positions\":[12,28]}}"
    ));
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
            debug_attrs("qk"),
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
    assert!(text.contains("qk [hlo.matmul]"));
    assert!(text.contains("<- lhs"));
    assert!(text.contains("-> out"));

    let dot =
        render_dot(&graph, GraphRenderOptions { show_types: true, show_attrs: true })
            .unwrap();
    assert!(dot.starts_with("digraph ir"));
    assert!(dot.contains("rankdir=TB"));
    assert!(dot.contains("hlo.matmul"));
    assert!(dot.contains("%lhs"));
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
