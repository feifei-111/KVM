use crate::compiler::context::Context;
use crate::compiler::dialect::OperatorIndex;
use crate::compiler::ir::OperationIndex;

pub trait OpView: Sized {
    fn operator(context: &Context) -> Option<OperatorIndex>;

    fn from_operation(context: &Context, operation: OperationIndex) -> Option<Self>;

    fn operation(&self) -> OperationIndex;
}
