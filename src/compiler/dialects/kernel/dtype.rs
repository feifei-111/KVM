use std::fmt;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DType {
    F16,
    F32,
    I32,
}

impl DType {
    fn as_symbol(self) -> &'static str {
        match self {
            Self::F16 => "f16",
            Self::F32 => "f32",
            Self::I32 => "i32",
        }
    }
}

impl fmt::Display for DType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_symbol())
    }
}
