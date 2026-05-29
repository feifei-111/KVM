use std::collections::BTreeMap;

use super::Graph;
use crate::compiler::ir::attr::format_attr_map;
use crate::compiler::ir::text::{find_top_level_char, split_top_level};
use crate::compiler::ir::{Attr, AttrMap, IrError, Type, TypeExpr, Value};

impl Graph {
    pub fn to_text(&self) -> Result<String, IrError> {
        let mut out = String::new();
        let names = self.value_names();
        for (index, value) in self.values.iter().enumerate() {
            if value.producer.is_some() {
                continue;
            }
            out.push_str(&format!("{} : {}", names[index], self.type_expr(value.ty)?));
            if !value.attrs.is_empty() {
                out.push(' ');
                out.push_str(&format_attr_map(&value.attrs));
            }
            out.push('\n');
        }
        if !self.operations.iter().all(|op| op.erased) {
            out.push('\n');
        }
        for op in self.topological_operations()? {
            let data = &self.operations[op.index()];
            let result_names = data
                .results
                .iter()
                .map(|value| names[value.index()].as_str())
                .collect::<Vec<_>>()
                .join(", ");
            let operand_names = data
                .operands
                .iter()
                .map(|value| names[value.index()].as_str())
                .collect::<Vec<_>>()
                .join(", ");
            let result_types = data
                .results
                .iter()
                .map(|value| {
                    self.type_expr(self.values[value.index()].ty)
                        .map(ToString::to_string)
                })
                .collect::<Result<Vec<_>, _>>()?
                .join(", ");
            out.push_str(&format!(
                "{result_names} = {}({operand_names})\n    -> {result_types}",
                self.type_expr(data.ty)?
            ));
            if !data.attrs.is_empty() {
                out.push('\n');
                out.push_str("    ");
                out.push_str(&format_attr_map(&data.attrs));
            }
            out.push('\n');
        }
        Ok(out.trim_end().to_string())
    }

    pub fn parse_text(text: &str) -> Result<Self, IrError> {
        let mut graph = Self::new();
        let mut type_cache = BTreeMap::<TypeExpr, Type>::new();
        let mut values = BTreeMap::<String, Value>::new();
        let lines = text
            .lines()
            .map(str::trim)
            .filter(|line| !line.is_empty())
            .collect::<Vec<_>>();
        let mut index = 0;
        while index < lines.len() {
            let line = lines[index];
            if line.contains(" = ") {
                parse_operation_line(
                    &mut graph,
                    &mut type_cache,
                    &mut values,
                    &lines,
                    &mut index,
                )?;
            } else {
                parse_value_line(&mut graph, &mut type_cache, &mut values, line)?;
            }
            index += 1;
        }
        Ok(graph)
    }
}

fn parse_operation_line(
    graph: &mut Graph,
    type_cache: &mut BTreeMap<TypeExpr, Type>,
    values: &mut BTreeMap<String, Value>,
    lines: &[&str],
    index: &mut usize,
) -> Result<(), IrError> {
    let line = lines[*index];
    let (lhs, rest) = line
        .split_once(" = ")
        .ok_or_else(|| IrError::Parse(format!("invalid operation: {line}")))?;
    let open = rest
        .find('(')
        .ok_or_else(|| IrError::Parse(format!("missing '(' in operation: {line}")))?;
    let close = rest
        .rfind(')')
        .ok_or_else(|| IrError::Parse(format!("missing ')' in operation: {line}")))?;
    let op_ty_expr = TypeExpr::parse(&rest[..open])?;
    let operands = split_top_level(&rest[open + 1..close], ',')
        .into_iter()
        .filter(|name| !name.trim().is_empty())
        .map(|name| {
            values.get(name.trim()).copied().ok_or_else(|| {
                IrError::Parse(format!("unknown operand {}", name.trim()))
            })
        })
        .collect::<Result<Vec<_>, _>>()?;

    *index += 1;
    let result_line = lines.get(*index).ok_or_else(|| {
        IrError::Parse(format!("missing result type after operation: {line}"))
    })?;
    let result_text = result_line.strip_prefix("->").ok_or_else(|| {
        IrError::Parse(format!(
            "operation result line must start with ->: {result_line}"
        ))
    })?;
    let result_types = split_top_level(result_text, ',')
        .into_iter()
        .map(|ty| Ok(intern_type(graph, type_cache, TypeExpr::parse(ty.trim())?)))
        .collect::<Result<Vec<_>, _>>()?;
    let mut attrs = AttrMap::new();
    if let Some(next) = lines.get(*index + 1) {
        if next.starts_with('{') {
            attrs = Attr::parse(next)?
                .into_dict()
                .ok_or_else(|| IrError::Parse(format!("attrs must be dict: {next}")))?;
            *index += 1;
        }
    }
    let op_ty = intern_type(graph, type_cache, op_ty_expr);
    let op = graph.add_named_operation(
        None::<String>,
        op_ty,
        operands,
        result_types,
        attrs,
    )?;
    let result_values = graph.results(op)?.to_vec();
    let lhs_names = split_top_level(lhs, ',');
    if lhs_names.len() != result_values.len() {
        return Err(IrError::Parse(format!(
            "result name count does not match result type count: {line}"
        )));
    }
    for (name, value) in lhs_names.into_iter().zip(result_values) {
        let name = name.trim().to_string();
        graph.set_value_name(value, name.clone())?;
        values.insert(name, value);
    }
    Ok(())
}

fn parse_value_line(
    graph: &mut Graph,
    type_cache: &mut BTreeMap<TypeExpr, Type>,
    values: &mut BTreeMap<String, Value>,
    line: &str,
) -> Result<(), IrError> {
    let (name, rest) = line
        .split_once(':')
        .ok_or_else(|| IrError::Parse(format!("invalid value line: {line}")))?;
    let (ty_text, attrs) = split_type_and_optional_attrs(rest.trim())?;
    let ty = intern_type(graph, type_cache, TypeExpr::parse(ty_text)?);
    let value = graph.add_named_input(Some(name.trim().to_string()), ty, attrs)?;
    values.insert(name.trim().to_string(), value);
    Ok(())
}

fn intern_type(
    graph: &mut Graph,
    cache: &mut BTreeMap<TypeExpr, Type>,
    ty: TypeExpr,
) -> Type {
    if let Some(existing) = cache.get(&ty) {
        return *existing;
    }
    let id = graph.define_type(ty.clone());
    cache.insert(ty, id);
    id
}

fn split_type_and_optional_attrs(input: &str) -> Result<(&str, AttrMap), IrError> {
    if let Some(open) = find_top_level_char(input, '{') {
        let ty = input[..open].trim();
        let attrs = Attr::parse(input[open..].trim())?
            .into_dict()
            .ok_or_else(|| IrError::Parse(format!("attrs must be dict: {input}")))?;
        Ok((ty, attrs))
    } else {
        Ok((input.trim(), AttrMap::new()))
    }
}
