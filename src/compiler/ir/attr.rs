use std::collections::BTreeMap;
use std::fmt;
use std::fmt::Write as _;

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
        if input == "null" {
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
            Self::Dict(values) => write!(f, "{}", format_attr_map(values)),
        }
    }
}

pub fn format_attr_map(values: &AttrMap) -> String {
    let mut out = String::new();
    write_attr_map_json(values, &mut out);
    out
}

pub fn format_attr_map_pretty(values: &AttrMap) -> String {
    let mut out = String::new();
    write_attr_map_json_pretty(values, 0, &mut out);
    out
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
        let (key, value) = split_key_value(item)?;
        values.insert(parse_key(key.trim())?, Attr::parse(value.trim())?);
    }
    Ok(Attr::Dict(values))
}

fn parse_string(input: &str) -> Result<String, IrError> {
    if !input.ends_with('"') || input.len() < 2 {
        return Err(IrError::Parse(format!("string missing quote: {input}")));
    }
    let mut out = String::new();
    let mut chars = input[1..input.len() - 1].chars();
    while let Some(ch) = chars.next() {
        if ch != '\\' {
            out.push(ch);
            continue;
        }
        let escaped = chars
            .next()
            .ok_or_else(|| IrError::Parse(format!("invalid string escape: {input}")))?;
        match escaped {
            '"' => out.push('"'),
            '\\' => out.push('\\'),
            'n' => out.push('\n'),
            'r' => out.push('\r'),
            't' => out.push('\t'),
            other => {
                out.push('\\');
                out.push(other);
            }
        }
    }
    Ok(out)
}

fn looks_like_float(input: &str) -> bool {
    input.contains('.') && input.parse::<f64>().is_ok()
}

fn split_key_value(input: &str) -> Result<(&str, &str), IrError> {
    let mut square = 0usize;
    let mut brace = 0usize;
    let mut paren = 0usize;
    let mut in_string = false;
    let mut escaped = false;
    for (index, ch) in input.char_indices() {
        if in_string {
            if escaped {
                escaped = false;
            } else if ch == '\\' {
                escaped = true;
            } else if ch == '"' {
                in_string = false;
            }
            continue;
        }
        match ch {
            '"' => in_string = true,
            '[' => square += 1,
            ']' => square = square.saturating_sub(1),
            '{' => brace += 1,
            '}' => brace = brace.saturating_sub(1),
            '(' => paren += 1,
            ')' => paren = paren.saturating_sub(1),
            ':' | '=' if square == 0 && brace == 0 && paren == 0 => {
                return Ok((&input[..index], &input[index + ch.len_utf8()..]));
            }
            _ => {}
        }
    }
    Err(IrError::Parse(format!("dict item must be key:value: {input}")))
}

fn parse_key(input: &str) -> Result<String, IrError> {
    if input.starts_with('"') {
        parse_string(input)
    } else if input.is_empty() {
        Err(IrError::Parse("empty dict key".to_string()))
    } else {
        Ok(input.to_string())
    }
}

fn write_attr_json(value: &Attr, out: &mut String) {
    match value {
        Attr::Unit => out.push_str("null"),
        Attr::Bool(value) => {
            let _ = write!(out, "{value}");
        }
        Attr::Int(value) => {
            let _ = write!(out, "{value}");
        }
        Attr::Float(value) => out.push_str(value),
        Attr::String(value) | Attr::Symbol(value) => write_json_string(value, out),
        Attr::List(values) => {
            out.push('[');
            for (index, value) in values.iter().enumerate() {
                if index > 0 {
                    out.push(',');
                }
                write_attr_json(value, out);
            }
            out.push(']');
        }
        Attr::Dict(values) => write_attr_map_json(values, out),
    }
}

fn write_attr_map_json(values: &AttrMap, out: &mut String) {
    out.push('{');
    for (index, (key, value)) in values.iter().enumerate() {
        if index > 0 {
            out.push(',');
        }
        write_json_string(key, out);
        out.push(':');
        write_attr_json(value, out);
    }
    out.push('}');
}

fn write_attr_json_pretty(value: &Attr, indent: usize, out: &mut String) {
    match value {
        Attr::List(values) => write_attr_list_json_pretty(values, indent, out),
        Attr::Dict(values) => write_attr_map_json_pretty(values, indent, out),
        _ => write_attr_json(value, out),
    }
}

fn write_attr_map_json_pretty(values: &AttrMap, indent: usize, out: &mut String) {
    if values.is_empty() {
        out.push_str("{}");
        return;
    }
    out.push('{');
    for (index, (key, value)) in values.iter().enumerate() {
        if index > 0 {
            out.push(',');
        }
        out.push('\n');
        write_indent(indent + 2, out);
        write_json_string(key, out);
        out.push_str(": ");
        write_attr_json_pretty(value, indent + 2, out);
    }
    out.push('\n');
    write_indent(indent, out);
    out.push('}');
}

fn write_attr_list_json_pretty(values: &[Attr], indent: usize, out: &mut String) {
    if values.is_empty() {
        out.push_str("[]");
        return;
    }
    if values.iter().all(is_number_attr) {
        out.push('[');
        for (index, value) in values.iter().enumerate() {
            if index > 0 {
                out.push_str(", ");
            }
            write_attr_json(value, out);
        }
        out.push(']');
        return;
    }
    out.push('[');
    for (index, value) in values.iter().enumerate() {
        if index > 0 {
            out.push(',');
        }
        out.push('\n');
        write_indent(indent + 2, out);
        write_attr_json_pretty(value, indent + 2, out);
    }
    out.push('\n');
    write_indent(indent, out);
    out.push(']');
}

fn is_number_attr(value: &Attr) -> bool {
    matches!(value, Attr::Int(_) | Attr::Float(_))
}

fn write_indent(indent: usize, out: &mut String) {
    for _ in 0..indent {
        out.push(' ');
    }
}

fn write_json_string(input: &str, out: &mut String) {
    out.push('"');
    for ch in input.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            other => out.push(other),
        }
    }
    out.push('"');
}
