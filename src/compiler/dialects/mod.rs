mod description;

pub mod hlo;
pub mod llo;

pub use description::{
    ArgumentDescription, AttrDescription, DialectDescription, OperationDescription,
    ValueTypeDescription,
};

pub fn all_descriptions() -> Vec<DialectDescription> {
    vec![hlo::description(), llo::description()]
}

pub fn find_description(name: &str) -> Option<DialectDescription> {
    all_descriptions().into_iter().find(|description| description.name == name)
}
