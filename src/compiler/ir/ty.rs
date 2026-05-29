use std::collections::BTreeMap;
use std::fmt;
use std::str::FromStr;

use super::text::split_top_level;
use super::{Attr, IrError};

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct TypeExpr {
    dialect: String,
    kind: String,
    params: BTreeMap<String, Attr>,
}

impl TypeExpr {
    pub fn new(dialect: impl Into<String>, kind: impl Into<String>) -> Self {
        Self { dialect: dialect.into(), kind: kind.into(), params: BTreeMap::new() }
    }

    pub fn with_param(mut self, key: impl Into<String>, value: Attr) -> Self {
        self.params.insert(key.into(), value);
        self
    }

    pub fn dialect(&self) -> &str {
        &self.dialect
    }

    pub fn kind(&self) -> &str {
        &self.kind
    }

    pub fn params(&self) -> &BTreeMap<String, Attr> {
        &self.params
    }

    pub fn parse(input: &str) -> Result<Self, IrError> {
        let input = input.trim();
        let (head, params) = if let Some(open) = input.find('<') {
            let close = input.rfind('>').ok_or_else(|| {
                IrError::Parse(format!("missing '>' in type: {input}"))
            })?;
            if close != input.len() - 1 {
                return Err(IrError::Parse(format!(
                    "unexpected trailing text in type: {input}"
                )));
            }
            (&input[..open], Some(&input[open + 1..close]))
        } else {
            (input, None)
        };
        let (dialect, kind) = head.split_once('.').ok_or_else(|| {
            IrError::Parse(format!("type must be dialect.kind: {input}"))
        })?;
        if dialect.is_empty() || kind.is_empty() {
            return Err(IrError::Parse(format!("empty dialect or kind: {input}")));
        }

        let mut ty = Self::new(dialect, kind);
        if let Some(params) = params {
            for item in split_top_level(params, ',') {
                let item = item.trim();
                if item.is_empty() {
                    continue;
                }
                let (key, value) = item.split_once('=').ok_or_else(|| {
                    IrError::Parse(format!("type param must be key=value: {item}"))
                })?;
                ty.params.insert(key.trim().to_string(), Attr::parse(value.trim())?);
            }
        }
        Ok(ty)
    }
}

impl fmt::Display for TypeExpr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}", self.dialect, self.kind)?;
        if !self.params.is_empty() {
            write!(f, "<")?;
            for (i, (key, value)) in self.params.iter().enumerate() {
                if i > 0 {
                    write!(f, ", ")?;
                }
                write!(f, "{key}={value}")?;
            }
            write!(f, ">")?;
        }
        Ok(())
    }
}

impl FromStr for TypeExpr {
    type Err = IrError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::parse(s)
    }
}
