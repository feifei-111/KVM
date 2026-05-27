pub mod dialect;
pub mod hir;
pub mod key;

pub use dialect::Dialect;
pub use key::DialectKey;

#[cfg(test)]
mod tests;
