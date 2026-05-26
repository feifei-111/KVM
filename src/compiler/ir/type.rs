use std::any::Any;

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct TypeName {
    pub dialect: String,
    pub name: String,
}

impl TypeName {
    pub fn new(dialect: impl Into<String>, name: impl Into<String>) -> Self {
        return Self { dialect: dialect.into(), name: name.into() };
    }
}

pub trait Type: std::fmt::Debug {
    fn name(&self) -> TypeName;
    fn as_any(&self) -> &dyn Any;
}
