use std::collections::BTreeMap;

use crate::compiler::ir::{Index, IndexKind};

pub trait Storable {
    type Kind: IndexKind;
}

#[derive(Clone, Debug)]
pub struct Store<Component: Storable> {
    next_index: usize,
    values: BTreeMap<Index<Component::Kind>, Component>,
}

impl<Component: Storable> Store<Component> {
    pub fn new() -> Self {
        return Self { next_index: 0, values: BTreeMap::new() };
    }

    pub fn insert(&mut self, value: Component) -> Index<Component::Kind> {
        let index = Index::new(self.next_index);
        self.next_index += 1;
        self.values.insert(index, value);
        return index;
    }

    pub fn get(&self, index: Index<Component::Kind>) -> Option<&Component> {
        return self.values.get(&index);
    }

    pub fn get_mut(&mut self, index: Index<Component::Kind>) -> Option<&mut Component> {
        return self.values.get_mut(&index);
    }

    pub fn contains(&self, index: Index<Component::Kind>) -> bool {
        return self.values.contains_key(&index);
    }

    pub fn remove(&mut self, index: Index<Component::Kind>) -> Option<Component> {
        return self.values.remove(&index);
    }
}

impl<Component: Storable> Default for Store<Component> {
    fn default() -> Self {
        return Self::new();
    }
}
