use crate::compiler::ir::Attr;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Shape(Vec<u64>);

impl Shape {
    pub fn new(dims: impl Into<Vec<u64>>) -> Self {
        Self(dims.into())
    }

    pub fn dims(&self) -> &[u64] {
        &self.0
    }

    pub(crate) fn as_attr(&self) -> Attr {
        Attr::List(self.0.iter().map(|dim| Attr::Int(*dim as i64)).collect())
    }
}

impl<const N: usize> From<[u64; N]> for Shape {
    fn from(value: [u64; N]) -> Self {
        Self(value.to_vec())
    }
}

impl<const N: usize> From<[usize; N]> for Shape {
    fn from(value: [usize; N]) -> Self {
        Self(value.iter().map(|dim| *dim as u64).collect())
    }
}
