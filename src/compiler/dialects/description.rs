#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DialectDescription {
    pub name: &'static str,
    pub graph_properties: &'static [AttrDescription],
    pub value_types: &'static [ValueTypeDescription],
    pub operations: &'static [OperationDescription],
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ValueTypeDescription {
    pub name: &'static str,
    pub attrs: &'static [AttrDescription],
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OperationDescription {
    pub name: &'static str,
    pub attrs: &'static [AttrDescription],
    pub operands: &'static [ArgumentDescription],
    pub results: &'static [ArgumentDescription],
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AttrDescription {
    pub name: &'static str,
    pub ty: &'static str,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArgumentDescription {
    pub ty: &'static str,
}
