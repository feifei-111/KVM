mod builder;
mod dtype;
mod matmul;
mod shape;
mod tensor;

pub use builder::KernelBuilder;
pub use dtype::DType;
pub use matmul::MatmulType;
pub use shape::Shape;
pub use tensor::{Layout, TensorType};

pub const DIALECT: &str = "kernel";
