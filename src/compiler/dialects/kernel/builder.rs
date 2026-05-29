use crate::compiler::dialects::kernel::{DType, MatmulType, TensorType};
use crate::compiler::ir::{AttrMap, Graph, IrError, Type, Value};

pub struct KernelBuilder<'g> {
    graph: &'g mut Graph,
}

impl<'g> KernelBuilder<'g> {
    pub fn new(graph: &'g mut Graph) -> Self {
        Self { graph }
    }

    pub fn tensor_type(&mut self, ty: TensorType) -> Type {
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

    pub fn matmul(
        &mut self,
        lhs: Value,
        rhs: Value,
        result: Type,
        accum: DType,
    ) -> Result<Value, IrError> {
        self.matmul_with_attrs(lhs, rhs, result, accum, AttrMap::new())
    }

    pub fn matmul_with_attrs(
        &mut self,
        lhs: Value,
        rhs: Value,
        result: Type,
        accum: DType,
        attrs: AttrMap,
    ) -> Result<Value, IrError> {
        let op_ty = self.graph.define_type(MatmulType::new(accum).into());
        let op = self.graph.add_operation(op_ty, [lhs, rhs], [result], attrs)?;
        Ok(self.graph.results(op)?[0])
    }
}
