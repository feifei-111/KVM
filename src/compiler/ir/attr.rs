use std::collections::BTreeMap;
use std::fmt;

use super::IrError;
use super::text::split_top_level;

pub type AttrMap = BTreeMap<String, Attr>;

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub enum Attr {
    Unit,
    Bool(bool),
    Int(i64),
    Float(String),
    String(String),
    Symbol(String),
    List(Vec<Attr>),
    Dict(AttrMap),
}

impl Attr {
    pub fn parse(input: &str) -> Result<Self, IrError> {
        let input = input.trim();
        if input.is_empty() {
            return Err(IrError::Parse("empty attr".to_string()));
        }
        if input == "unit" {
            return Ok(Self::Unit);
        }
        if input == "true" {
            return Ok(Self::Bool(true));
        }
        if input == "false" {
            return Ok(Self::Bool(false));
        }
        if input.starts_with('"') {
            return parse_string(input).map(Self::String);
        }
        if input.starts_with('[') {
            return parse_list(input);
        }
        if input.starts_with('{') {
            return parse_dict(input);
        }
        if let Ok(value) = input.parse::<i64>() {
            return Ok(Self::Int(value));
        }
        if looks_like_float(input) {
            return Ok(Self::Float(input.to_string()));
        }
        Ok(Self::Symbol(input.to_string()))
    }

    pub(crate) fn into_dict(self) -> Option<AttrMap> {
        match self {
            Self::Dict(values) => Some(values),
            _ => None,
        }
    }
}

impl fmt::Display for Attr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Unit => write!(f, "unit"),
            Self::Bool(value) => write!(f, "{value}"),
            Self::Int(value) => write!(f, "{value}"),
            Self::Float(value) => write!(f, "{value}"),
            Self::String(value) => write!(f, "\"{}\"", value.replace('"', "\\\"")),
            Self::Symbol(value) => write!(f, "{value}"),
            Self::List(values) => {
                write!(f, "[")?;
                for (i, value) in values.iter().enumerate() {
                    if i > 0 {
                        write!(f, ",")?;
                    }
                    write!(f, "{value}")?;
                }
                write!(f, "]")
            }
            Self::Dict(values) => write_attr_map(f, values),
        }
    }
}

pub(crate) fn format_attr_map(values: &AttrMap) -> String {
    format!("{}", Attr::Dict(values.clone()))
}

fn write_attr_map(f: &mut fmt::Formatter<'_>, values: &AttrMap) -> fmt::Result {
    write!(f, "{{ ")?;
    for (i, (key, value)) in values.iter().enumerate() {
        if i > 0 {
            write!(f, ", ")?;
        }
        write!(f, "{key} = {value}")?;
    }
    write!(f, " }}")
}

fn parse_list(input: &str) -> Result<Attr, IrError> {
    if !input.ends_with(']') {
        return Err(IrError::Parse(format!("list missing ']': {input}")));
    }
    let body = &input[1..input.len() - 1];
    let values = split_top_level(body, ',')
        .into_iter()
        .filter(|item| !item.trim().is_empty())
        .map(|item| Attr::parse(item.trim()))
        .collect::<Result<Vec<_>, _>>()?;
    Ok(Attr::List(values))
}

fn parse_dict(input: &str) -> Result<Attr, IrError> {
    if !input.ends_with('}') {
        return Err(IrError::Parse(format!("dict missing '}}': {input}")));
    }
    let body = &input[1..input.len() - 1];
    let mut values = AttrMap::new();
    for item in split_top_level(body, ',') {
        let item = item.trim();
        if item.is_empty() {
            continue;
        }
        let (key, value) = item.split_once('=').ok_or_else(|| {
            IrError::Parse(format!("dict item must be key=value: {item}"))
        })?;
        values.insert(key.trim().to_string(), Attr::parse(value.trim())?);
    }
    Ok(Attr::Dict(values))
}

fn parse_string(input: &str) -> Result<String, IrError> {
    if !input.ends_with('"') || input.len() < 2 {
        return Err(IrError::Parse(format!("string missing quote: {input}")));
    }
    Ok(input[1..input.len() - 1].replace("\\\"", "\""))
}

fn looks_like_float(input: &str) -> bool {
    input.contains('.') && input.parse::<f64>().is_ok()
}
