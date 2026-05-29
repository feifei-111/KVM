use crate::compiler::dialects::hlo::{
    ArithBinaryOp, ArithBinaryType, ArithCastType, ArithComparePredicate,
    ArithCompareType, ArithUnaryOp, ArithUnaryType, AttentionOp, AttentionType,
    AttnMetaType, BroadcastType, CollectiveOp, CollectiveType, CombineType, DType,
    DispatchType, DistConfig, DistDesc, GemmType, KvCacheAccess, KvCacheOpType,
    KvCacheType, MatmulType, RecvType, SendType, TensorType, TokenType,
    set_dist_config,
};
use crate::compiler::ir::{AttrMap, Graph, IrError, Type, Value};

pub struct HloBuilder<'g> {
    graph: &'g mut Graph,
}

impl<'g> HloBuilder<'g> {
    pub fn new(graph: &'g mut Graph) -> Self {
        Self { graph }
    }

    pub fn set_dist_config(&mut self, config: DistConfig) {
        set_dist_config(self.graph, config);
    }

    pub fn tensor_type(&mut self, ty: TensorType) -> Type {
        self.graph.define_type(ty.into())
    }

    pub fn token_type(&mut self) -> Type {
        self.graph.define_type(TokenType.into())
    }

    pub fn attn_meta_type(&mut self) -> Type {
        self.graph.define_type(AttnMetaType.into())
    }

    pub fn kvcache_type(&mut self, ty: KvCacheType) -> Type {
        self.graph.define_type(ty.into())
    }

    pub fn input(
        &mut self,
        name: impl Into<String>,
        ty: Type,
    ) -> Result<Value, IrError> {
        self.graph.add_named_input(Some(name.into()), ty, AttrMap::new())
    }

    pub fn constant(&mut self, ty: Type, attrs: AttrMap) -> Result<Value, IrError> {
        self.graph.add_constant(ty, attrs)
    }

    pub fn set_name(
        &mut self,
        value: Value,
        name: impl Into<String>,
    ) -> Result<(), IrError> {
        self.graph.set_value_name(value, name)
    }

    pub fn matmul(
        &mut self,
        lhs: Value,
        rhs: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        self.matmul_with_attrs(lhs, rhs, result, AttrMap::new())
    }

    pub fn matmul_with_attrs(
        &mut self,
        lhs: Value,
        rhs: Value,
        result: Type,
        attrs: AttrMap,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(MatmulType::new().into());
        let op = self.graph.add_operation(op_ty, [lhs, rhs], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn gemm(
        &mut self,
        lhs: Value,
        rhs: Value,
        acc: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(GemmType::new().into());
        let op = self.graph.add_operation(
            op_ty,
            [lhs, rhs, acc],
            [result],
            AttrMap::new(),
        )?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn attention_prefill(
        &mut self,
        query: Value,
        key: Value,
        value: Value,
        mask: Value,
        meta: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        self.attention(AttentionOp::Prefill, [query, key, value, mask, meta], result)
    }

    pub fn attention_decode(
        &mut self,
        query: Value,
        key_cache: Value,
        value_cache: Value,
        meta: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        self.attention(
            AttentionOp::Decode,
            [query, key_cache, value_cache, meta],
            result,
        )
    }

    pub fn attention_rope(
        &mut self,
        input: Value,
        meta: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty =
            self.graph.define_type(AttentionType::new(AttentionOp::Rope).into());
        let op =
            self.graph.add_operation(op_ty, [input, meta], [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn arith_binary(
        &mut self,
        op: ArithBinaryOp,
        lhs: Value,
        rhs: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(ArithBinaryType::new(op).into());
        let op =
            self.graph.add_operation(op_ty, [lhs, rhs], [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn arith_unary(
        &mut self,
        op: ArithUnaryOp,
        input: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(ArithUnaryType::new(op).into());
        let op = self.graph.add_operation(op_ty, [input], [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn arith_compare(
        &mut self,
        predicate: ArithComparePredicate,
        lhs: Value,
        rhs: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(ArithCompareType::new(predicate).into());
        let op =
            self.graph.add_operation(op_ty, [lhs, rhs], [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn arith_cast(
        &mut self,
        input: Value,
        result: Type,
        to: DType,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(ArithCastType::new(to).into());
        let op = self.graph.add_operation(op_ty, [input], [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    fn attention<const N: usize>(
        &mut self,
        op: AttentionOp,
        operands: [Value; N],
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(AttentionType::new(op).into());
        let op = self.graph.add_operation(op_ty, operands, [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn collective(
        &mut self,
        op: CollectiveOp,
        desc: DistDesc,
        input: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_type = CollectiveType::new(op, desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let op = self.graph.add_operation(op_ty, [input], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn broadcast(
        &mut self,
        desc: DistDesc,
        input: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_type = BroadcastType::new(desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let op = self.graph.add_operation(op_ty, [input], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn dispatch(
        &mut self,
        desc: DistDesc,
        input: Value,
        routing: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_type = DispatchType::new(desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let op = self.graph.add_operation(op_ty, [input, routing], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn combine(
        &mut self,
        desc: DistDesc,
        input: Value,
        routing: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_type = CombineType::new(desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let op = self.graph.add_operation(op_ty, [input, routing], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn send(&mut self, desc: DistDesc, input: Value) -> Result<Value, IrError> {
        let op_type = SendType::new(desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let token_ty = self.token_type();
        let op = self.graph.add_operation(op_ty, [input], [token_ty], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn recv(
        &mut self,
        desc: DistDesc,
        dst: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        let op_type = RecvType::new(desc);
        let attrs = op_type.attrs();
        let op_ty = self.graph.define_type(op_type.into());
        let op = self.graph.add_operation(op_ty, [dst], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }

    pub fn kvcache_read(
        &mut self,
        cache: Value,
        meta: Value,
        key_result: Type,
        value_result: Type,
    ) -> Result<(Value, Value), IrError> {
        let op_ty =
            self.graph.define_type(KvCacheOpType::new(KvCacheAccess::Read).into());
        let op = self.graph.add_operation(
            op_ty,
            [cache, meta],
            [key_result, value_result],
            AttrMap::new(),
        )?;
        let results = self.graph.results(op)?;
        Ok((results[0], results[1]))
    }

    pub fn kvcache_write(
        &mut self,
        cache: Value,
        meta: Value,
        key: Value,
        value: Value,
        result: Type,
    ) -> Result<Value, IrError> {
        self.kvcache(KvCacheAccess::Write, [cache, meta, key, value], result)
    }

    fn kvcache<const N: usize>(
        &mut self,
        access: KvCacheAccess,
        operands: [Value; N],
        result: Type,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(KvCacheOpType::new(access).into());
        let op = self.graph.add_operation(op_ty, operands, [result], AttrMap::new())?;
        Ok(self.graph.results(op)?[0])
    }
}
