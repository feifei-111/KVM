use super::*;
use crate::compiler::ir::Graph;

fn tensor(builder: &mut HloBuilder<'_>) -> crate::compiler::ir::Type {
    builder.tensor_type(TensorType::new(DType::F16, [128usize, 128usize]))
}

#[test]
fn builder_creates_arith_ops() {
    let mut graph = Graph::new();
    let mut hlo = HloBuilder::new(&mut graph);
    let tensor = tensor(&mut hlo);
    let lhs = hlo.input("lhs", tensor).unwrap();
    let rhs = hlo.input("rhs", tensor).unwrap();

    let sum = hlo.arith_binary(ArithBinaryOp::Add, lhs, rhs, tensor).unwrap();
    let exp = hlo.arith_unary(ArithUnaryOp::Exp, sum, tensor).unwrap();
    let cast = hlo.arith_cast(exp, tensor, DType::F16).unwrap();
    hlo.set_name(cast, "cast").unwrap();

    graph.verify().unwrap();
    let text = graph.to_text().unwrap();
    assert!(text.contains("\"hlo.arith.add\"(%lhs, %rhs)"));
    assert!(text.contains("\"hlo.arith.exp\""));
    assert!(text.contains("%cast = \"hlo.arith.cast.f16\""));
}

#[test]
fn builder_creates_gemm_and_attention_ops() {
    let mut graph = Graph::new();
    let mut hlo = HloBuilder::new(&mut graph);
    let tensor = tensor(&mut hlo);
    let lhs = hlo.input("lhs", tensor).unwrap();
    let rhs = hlo.input("rhs", tensor).unwrap();
    let acc = hlo.input("acc", tensor).unwrap();
    let mask = hlo.input("mask", tensor).unwrap();
    let meta_ty = hlo.attn_meta_type();
    let meta = hlo.input("attn_meta", meta_ty).unwrap();

    let gemm = hlo.gemm(lhs, rhs, acc, tensor).unwrap();
    hlo.set_name(gemm, "gemm").unwrap();
    let q_rope = hlo.attention_rope(gemm, meta, tensor).unwrap();
    hlo.set_name(q_rope, "q_rope").unwrap();
    let prefill = hlo.attention_prefill(q_rope, lhs, rhs, mask, meta, tensor).unwrap();
    hlo.set_name(prefill, "prefill").unwrap();
    let decode = hlo.attention_decode(q_rope, lhs, rhs, meta, tensor).unwrap();
    hlo.set_name(decode, "decode").unwrap();

    graph.verify().unwrap();
    let text = graph.to_text().unwrap();
    assert!(text.contains("%gemm = \"hlo.gemm\"(%lhs, %rhs, %acc)"));
    assert!(text.contains("%q_rope = \"hlo.attn.rope\"(%gemm, %attn_meta)"));
    assert!(text.contains(
        "%prefill = \"hlo.attn.prefill\"(%q_rope, %lhs, %rhs, %mask, %attn_meta)"
    ));
    assert!(
        text.contains("%decode = \"hlo.attn.decode\"(%q_rope, %lhs, %rhs, %attn_meta)")
    );
}

#[test]
fn builder_creates_communication_ops() {
    let mut graph = Graph::new();
    let mut hlo = HloBuilder::new(&mut graph);
    hlo.set_dist_config(
        DistConfig::nccl()
            .with_rank(0, "node0", 0, "cuda:0")
            .with_rank(1, "node0", 1, "cuda:1"),
    );
    let tensor = tensor(&mut hlo);
    let input = hlo.input("x", tensor).unwrap();
    let routing = hlo.input("routing", tensor).unwrap();

    let reduced = hlo
        .collective(
            CollectiveOp::AllReduceSum,
            DistDesc::collective([0, 1]),
            input,
            tensor,
        )
        .unwrap();
    hlo.set_name(reduced, "reduced").unwrap();
    let dispatched =
        hlo.dispatch(DistDesc::collective([0, 1]), reduced, routing, tensor).unwrap();
    hlo.set_name(dispatched, "dispatched").unwrap();
    let combined =
        hlo.combine(DistDesc::collective([0, 1]), dispatched, routing, tensor).unwrap();
    hlo.set_name(combined, "combined").unwrap();
    let token = hlo.send(DistDesc::pair(0, 1), combined).unwrap();
    hlo.set_name(token, "sent").unwrap();
    let recv_dst = hlo.input("recv_dst", tensor).unwrap();
    let received = hlo.recv(DistDesc::pair(0, 1), recv_dst, tensor).unwrap();
    hlo.set_name(received, "received").unwrap();

    graph.verify().unwrap();
    let text = graph.to_text().unwrap();
    assert!(text.contains("graph.dist.config = {\"backend\":\"nccl\""));
    assert!(text.contains(
        "%reduced = \"hlo.comm.all_reduce.sum\"(%x) {\"dist\":{\"ranks\":[0,1]}}"
    ));
    assert!(
        text.contains("%dispatched = \"hlo.comm.dispatch\"(%reduced, %routing) {\"dist\":{\"ranks\":[0,1]}}")
    );
    assert!(
        text.contains("%combined = \"hlo.comm.combine\"(%dispatched, %routing) {\"dist\":{\"ranks\":[0,1]}}")
    );
    assert!(text.contains(
        "%sent = \"hlo.comm.send\"(%combined) {\"dist\":{\"dst\":1,\"src\":0}}"
    ));
    assert!(text.contains("-> hlo.token"));
    assert!(text.contains("%recv_dst : hlo.tensor"));
    assert!(text.contains(
        "%received = \"hlo.comm.recv\"(%recv_dst) {\"dist\":{\"dst\":1,\"src\":0}}"
    ));
}

#[test]
fn builder_creates_kvcache_ops() {
    let mut graph = Graph::new();
    let mut hlo = HloBuilder::new(&mut graph);
    let tensor = tensor(&mut hlo);
    let meta_ty = hlo.attn_meta_type();
    let cache_ty = hlo.kvcache_type(KvCacheType::new(DType::F16));
    let cache = hlo.input("cache", cache_ty).unwrap();
    let meta = hlo.input("attn_meta", meta_ty).unwrap();
    let key = hlo.input("key", tensor).unwrap();
    let value = hlo.input("value", tensor).unwrap();

    let (key_read, value_read) = hlo.kvcache_read(cache, meta, tensor, tensor).unwrap();
    hlo.set_name(key_read, "key_read").unwrap();
    hlo.set_name(value_read, "value_read").unwrap();
    let written = hlo.kvcache_write(cache, meta, key, value, cache_ty).unwrap();
    hlo.set_name(written, "written").unwrap();

    graph.verify().unwrap();
    let text = graph.to_text().unwrap();
    assert!(text.contains("%cache : hlo.kvcache<block_shape=paged_vllm, dtype=f16>"));
    assert!(
        text.contains(
            "%key_read, %value_read = \"hlo.kvcache.read\"(%cache, %attn_meta)"
        )
    );
    assert!(text.contains(
        "%written = \"hlo.kvcache.write\"(%cache, %attn_meta, %key, %value)"
    ));
    assert!(!text.contains("allocate"));
    assert!(!text.contains("append"));
}
