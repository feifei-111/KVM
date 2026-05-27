use std::fmt;
use std::marker::PhantomData;

pub trait IndexKind: Clone + Copy + fmt::Debug + Eq + Ord + 'static {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct ValueKind;
impl IndexKind for ValueKind {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct OperationKind;
impl IndexKind for OperationKind {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct TypeKind;
impl IndexKind for TypeKind {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct AttributeKind;
impl IndexKind for AttributeKind {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct OperatorKind;
impl IndexKind for OperatorKind {}

pub struct Index<Kind: IndexKind> {
    raw: usize,
    _kind: PhantomData<Kind>,
}

impl<Kind: IndexKind> Index<Kind> {
    pub fn new(raw: usize) -> Self {
        return Self { raw, _kind: PhantomData };
    }

    pub fn raw(self) -> usize {
        return self.raw;
    }
}

impl<Kind: IndexKind> Copy for Index<Kind> {}

impl<Kind: IndexKind> Clone for Index<Kind> {
    fn clone(&self) -> Self {
        return *self;
    }
}

impl<Kind: IndexKind> fmt::Debug for Index<Kind> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return write!(f, "{}({})", std::any::type_name::<Kind>(), self.raw);
    }
}

impl<Kind: IndexKind> PartialEq for Index<Kind> {
    fn eq(&self, other: &Self) -> bool {
        return self.raw == other.raw;
    }
}

impl<Kind: IndexKind> Eq for Index<Kind> {}

impl<Kind: IndexKind> PartialOrd for Index<Kind> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        return Some(self.cmp(other));
    }
}

impl<Kind: IndexKind> Ord for Index<Kind> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        return self.raw.cmp(&other.raw);
    }
}

impl<Kind: IndexKind> std::hash::Hash for Index<Kind> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.raw.hash(state);
    }
}
