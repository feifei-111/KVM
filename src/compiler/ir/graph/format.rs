use std::collections::BTreeMap;

use super::Graph;
use crate::compiler::ir::attr::format_attr_map;
use crate::compiler::ir::text::{find_top_level_char, split_top_level};
use crate::compiler::ir::{Attr, AttrMap, IrError, Type, TypeExpr, Value};

impl Graph {
    pub fn to_text(&self) -> Result<String, IrError> {
        let mut out = String::new();
        let names = self.value_names();
        for (key, value) in &self.properties {
            out.push_str(&format!("graph.{key} = {value}\n"));
        }
        if !self.properties.is_empty()
            && (!self.values.is_empty() || !self.operations.iter().all(|op| op.erased))
        {
            out.push('\n');
        }
        if let Some(block) = self.entry_block {
            self.write_block_text(&mut out, block, &names)?;
            return Ok(out.trim_end().to_string());
        }
        for (index, value) in self.values.iter().enumerate() {
            if value.producer.is_some()
                || value.kind == super::ValueKind::OperationResult
            {
                continue;
            }
            out.push_str(&format!("%{} : {}", names[index], self.type_expr(value.ty)?));
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
            self.write_operation_text(&mut out, op, &names, "")?;
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
            if line.starts_with("graph.") {
                parse_property_line(&mut graph, line)?;
            } else if line.starts_with("func ") || line.starts_with("func.func ") {
                parse_block(
                    &mut graph,
                    &mut type_cache,
                    &mut values,
                    &lines,
                    &mut index,
                )?;
            } else if looks_like_operation_line(line) {
                parse_operation_line(&mut graph, &mut type_cache, &mut values, line)?;
            } else {
                parse_value_line(&mut graph, &mut type_cache, &mut values, line)?;
            }
            index += 1;
        }
        Ok(graph)
    }

    fn write_block_text(
        &self,
        out: &mut String,
        block: crate::compiler::ir::Block,
        names: &[String],
    ) -> Result<(), IrError> {
        let block = self.expect_block(block)?;
        let inputs = block
            .inputs
            .iter()
            .map(|value| {
                let data = &self.values[value.index()];
                let mut input = format!(
                    "%{}: {}",
                    names[value.index()],
                    self.format_value_type(*value)?
                );
                if !data.attrs.is_empty() {
                    input.push(' ');
                    input.push_str(&format_attr_map(&data.attrs));
                }
                Ok(input)
            })
            .collect::<Result<Vec<_>, IrError>>()?;
        let outputs = block
            .outputs
            .iter()
            .map(|value| self.format_value_type(*value))
            .collect::<Result<Vec<_>, IrError>>()?
            .join(", ");
        if inputs.len() <= 2 {
            out.push_str(&format!("func @{}({})", block.name, inputs.join(", ")));
        } else {
            out.push_str(&format!("func @{}(\n", block.name));
            for (index, input) in inputs.iter().enumerate() {
                let suffix = if index + 1 == inputs.len() { "" } else { "," };
                out.push_str(&format!("  {input}{suffix}\n"));
            }
            out.push(')');
        }
        if !outputs.is_empty() {
            if block.outputs.len() == 1 {
                out.push_str(&format!(" -> {outputs}"));
            } else {
                out.push_str(&format!(" -> ({outputs})"));
            }
        }
        if !block.attrs.is_empty() {
            out.push(' ');
            out.push_str(&format_attr_map(&block.attrs));
        }
        out.push_str(" {\n");

        for (index, value) in self.values.iter().enumerate() {
            if value.kind != super::ValueKind::Constant {
                continue;
            }
            out.push_str(&format!("  %{} = \"ir.constant\"()", names[index]));
            if !value.attrs.is_empty() {
                out.push(' ');
                out.push_str(&format_attr_map(&value.attrs));
            }
            out.push_str(&format!(
                " : () -> {}",
                self.format_value_type(crate::compiler::ir::Value::new(index))?
            ));
            out.push('\n');
        }
        if self.values.iter().any(|value| value.kind == super::ValueKind::Constant)
            && !self.operations.iter().all(|op| op.erased)
        {
            out.push('\n');
        }

        for op in self.topological_operations()? {
            self.write_operation_text(out, op, names, "  ")?;
            out.push('\n');
        }
        if !block.outputs.is_empty() {
            let returned_values = block
                .outputs
                .iter()
                .map(|value| format!("%{}", names[value.index()]))
                .collect::<Vec<_>>()
                .join(", ");
            let returned_types = block
                .outputs
                .iter()
                .map(|value| self.format_value_type(*value))
                .collect::<Result<Vec<_>, _>>()?
                .join(", ");
            out.push_str(&format!("  return {returned_values} : {returned_types}\n"));
        }
        out.push('}');
        Ok(())
    }

    fn write_operation_text(
        &self,
        out: &mut String,
        op: crate::compiler::ir::Operation,
        names: &[String],
        indent: &str,
    ) -> Result<(), IrError> {
        let data = &self.operations[op.index()];
        let op_ty = self.type_expr(data.ty)?;
        let result_names = data
            .results
            .iter()
            .map(|value| format!("%{}", names[value.index()]))
            .collect::<Vec<_>>()
            .join(", ");
        let operand_names = data
            .operands
            .iter()
            .map(|value| format!("%{}", names[value.index()]))
            .collect::<Vec<_>>()
            .join(", ");
        let operand_types = data
            .operands
            .iter()
            .map(|value| {
                self.type_expr(self.values[value.index()].ty).map(ToString::to_string)
            })
            .collect::<Result<Vec<_>, _>>()?
            .join(", ");
        let result_types = data
            .results
            .iter()
            .map(|value| self.format_value_type(*value))
            .collect::<Result<Vec<_>, _>>()?;
        let result_type_text = format_result_type_list(&result_types);

        out.push_str(indent);
        if !result_names.is_empty() {
            out.push_str(&result_names);
            out.push_str(" = ");
        }
        out.push_str(&format!(
            "\"{}.{}\"({operand_names})",
            op_ty.dialect(),
            op_ty.kind()
        ));
        if !data.attrs.is_empty() {
            out.push(' ');
            out.push_str(&format_attr_map(&data.attrs));
        }
        out.push_str(&format!(" : ({operand_types}) -> {result_type_text}"));
        Ok(())
    }

    fn format_value_type(
        &self,
        value: crate::compiler::ir::Value,
    ) -> Result<String, IrError> {
        let data = &self.values[value.index()];
        Ok(self.type_expr(data.ty)?.to_string())
    }
}

fn parse_property_line(graph: &mut Graph, line: &str) -> Result<(), IrError> {
    let (key, value) = line
        .split_once(" = ")
        .ok_or_else(|| IrError::Parse(format!("invalid graph property: {line}")))?;
    let key = key
        .strip_prefix("graph.")
        .ok_or_else(|| IrError::Parse(format!("invalid graph property: {line}")))?
        .trim();
    if key.is_empty() {
        return Err(IrError::Parse(format!("empty graph property key: {line}")));
    }
    graph.set_property(key.to_string(), Attr::parse(value.trim())?);
    Ok(())
}

fn parse_operation_line(
    graph: &mut Graph,
    type_cache: &mut BTreeMap<TypeExpr, Type>,
    values: &mut BTreeMap<String, Value>,
    line: &str,
) -> Result<(), IrError> {
    let (lhs, rest) = line
        .split_once(" = ")
        .ok_or_else(|| IrError::Parse(format!("invalid operation: {line}")))?;
    let (op_name, rest) = parse_quoted_name(rest.trim(), line)?;
    let op_ty_expr = TypeExpr::parse(&op_name)?;
    if !op_ty_expr.fields().is_empty() {
        return Err(IrError::Parse(format!(
            "operation type fields must be operation attrs: {line}"
        )));
    }
    let rest = rest.trim();
    let open = rest
        .find('(')
        .ok_or_else(|| IrError::Parse(format!("missing '(' in operation: {line}")))?;
    if open != 0 {
        return Err(IrError::Parse(format!(
            "operation operands must follow name: {line}"
        )));
    }
    let close = find_matching_delimiter(rest, open, '(', ')')?;
    let operands = split_top_level(&rest[open + 1..close], ',')
        .into_iter()
        .filter(|name| !name.trim().is_empty())
        .map(|name| {
            let name = parse_value_ref(name.trim())?;
            values
                .get(name)
                .copied()
                .ok_or_else(|| IrError::Parse(format!("unknown operand {name}")))
        })
        .collect::<Result<Vec<_>, _>>()?;
    let mut rest = rest[close + 1..].trim();

    let mut attrs = AttrMap::new();
    if rest.starts_with('{') {
        let close = find_matching_delimiter(rest, 0, '{', '}')?;
        attrs = Attr::parse(rest[..=close].trim())?
            .into_dict()
            .ok_or_else(|| IrError::Parse(format!("attrs must be dict: {line}")))?;
        rest = rest[close + 1..].trim();
    }

    let signature = rest
        .strip_prefix(':')
        .ok_or_else(|| IrError::Parse(format!("operation missing signature: {line}")))?
        .trim();
    let (_operand_type_exprs, result_type_exprs) = parse_function_type(signature)?;
    let result_types = result_type_exprs
        .into_iter()
        .map(|ty| intern_type(graph, type_cache, ty.ty))
        .collect::<Vec<_>>();

    let lhs_names = split_top_level(lhs, ',')
        .into_iter()
        .map(|name| parse_value_ref(name.trim()).map(ToString::to_string))
        .collect::<Result<Vec<_>, _>>()?;
    if lhs_names.len() != result_types.len() {
        return Err(IrError::Parse(format!(
            "result name count does not match result type count: {line}"
        )));
    }

    if op_ty_expr.dialect() == "ir" && op_ty_expr.kind() == "constant" {
        if !operands.is_empty() || result_types.len() != 1 {
            return Err(IrError::Parse(format!("invalid constant operation: {line}")));
        }
        let value = graph.add_named_constant(
            Some(lhs_names[0].clone()),
            result_types[0],
            attrs,
        )?;
        values.insert(lhs_names[0].clone(), value);
        return Ok(());
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
    for (name, value) in lhs_names.into_iter().zip(result_values) {
        graph.set_value_name(value, name.clone())?;
        values.insert(name, value);
    }
    Ok(())
}

fn parse_block(
    graph: &mut Graph,
    type_cache: &mut BTreeMap<TypeExpr, Type>,
    values: &mut BTreeMap<String, Value>,
    lines: &[&str],
    index: &mut usize,
) -> Result<(), IrError> {
    let mut header = lines[*index].to_string();
    while !header.trim_end().ends_with('{') {
        *index += 1;
        let next = lines.get(*index).ok_or_else(|| {
            IrError::Parse(format!("missing '{{' for block header: {header}"))
        })?;
        header.push(' ');
        header.push_str(next);
    }
    let (name, inputs, output_decls, attrs) = parse_block_header(&header)?;
    let parsed_inputs = inputs
        .into_iter()
        .map(|(name, ty, attrs)| {
            let ty_id = intern_type(graph, type_cache, ty.ty);
            Ok((name, ty_id, attrs))
        })
        .collect::<Result<Vec<_>, IrError>>()?;
    let block = graph.new_block(
        name,
        parsed_inputs.iter().map(|(name, ty, _)| (name.clone(), *ty)),
        attrs,
    )?;
    graph.set_entry_block(block)?;
    for (value, (_, _, attrs)) in
        graph.block_inputs(block)?.to_vec().into_iter().zip(parsed_inputs)
    {
        for (key, attr) in attrs {
            graph.set_value_attr(value, key, attr)?;
        }
        let name = graph
            .value(value)?
            .name
            .ok_or_else(|| IrError::Parse("block input missing name".to_string()))?
            .to_string();
        values.insert(name, value);
    }

    *index += 1;
    while *index < lines.len() {
        let line = lines[*index];
        if line == "}" {
            return Ok(());
        }
        if line.starts_with("return ") {
            let outputs = parse_return_line(graph, values, &output_decls, line)?;
            graph.set_block_outputs(block, outputs)?;
        } else if looks_like_operation_line(line) {
            parse_operation_line(graph, type_cache, values, line)?;
        } else {
            parse_value_line(graph, type_cache, values, line)?;
        }
        *index += 1;
    }
    Err(IrError::Parse(format!("missing closing '}}' for block {:?}", block)))
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct BlockOutputDecl {
    ty: TypeExpr,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct TypeUse {
    ty: TypeExpr,
}

type BlockInputDecl = (String, TypeUse, AttrMap);
type BlockHeader = (String, Vec<BlockInputDecl>, Vec<BlockOutputDecl>, AttrMap);

fn parse_block_header(line: &str) -> Result<BlockHeader, IrError> {
    let header = line
        .strip_prefix("func.func ")
        .or_else(|| line.strip_prefix("func "))
        .ok_or_else(|| IrError::Parse(format!("invalid block header: {line}")))?;
    let header = header
        .strip_suffix('{')
        .ok_or_else(|| {
            IrError::Parse(format!("block header must end with '{{': {line}"))
        })?
        .trim();
    let open = header.find('(').ok_or_else(|| {
        IrError::Parse(format!("missing '(' in block header: {line}"))
    })?;
    let close = find_matching_delimiter(header, open, '(', ')')?;
    if close < open {
        return Err(IrError::Parse(format!("invalid block header: {line}")));
    }
    let name = header[..open].trim().strip_prefix('@').ok_or_else(|| {
        IrError::Parse(format!("function name must start with @: {line}"))
    })?;
    if name.is_empty() {
        return Err(IrError::Parse(format!("empty block name: {line}")));
    }
    let inputs = split_top_level(&header[open + 1..close], ',')
        .into_iter()
        .filter(|input| !input.trim().is_empty())
        .map(|input| {
            let (name, rest) = input.trim().split_once(':').ok_or_else(|| {
                IrError::Parse(format!("block input must be name: type: {input}"))
            })?;
            let (ty_text, attrs) = split_type_and_optional_attrs(rest.trim())?;
            Ok((
                parse_value_ref(name.trim())?.to_string(),
                parse_type_use(ty_text.trim())?,
                attrs,
            ))
        })
        .collect::<Result<Vec<_>, IrError>>()?;

    let rest = header[close + 1..].trim();
    let (output_text, attrs) = if rest.is_empty() {
        ("", AttrMap::new())
    } else {
        let rest = rest
            .strip_prefix("->")
            .ok_or_else(|| IrError::Parse(format!("invalid block outputs: {line}")))?
            .trim();
        (rest, AttrMap::new())
    };
    let outputs = parse_result_type_uses(output_text)?
        .into_iter()
        .map(|ty| BlockOutputDecl { ty: ty.ty })
        .collect::<Vec<_>>();
    Ok((name.to_string(), inputs, outputs, attrs))
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
    let name = parse_value_ref(name.trim())?.to_string();
    let value = graph.add_named_input(Some(name.clone()), ty, attrs)?;
    values.insert(name, value);
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
        let attrs_text = input[open..].trim();
        if attrs_text.is_empty() {
            return Ok((ty, AttrMap::new()));
        }
        if !attrs_text.starts_with('{') {
            return Err(IrError::Parse(format!("attrs must be dict: {input}")));
        }
        let close = find_matching_delimiter(attrs_text, 0, '{', '}')?;
        if close + 1 != attrs_text.len() {
            return Err(IrError::Parse(format!(
                "unexpected trailing text after attrs: {input}"
            )));
        }
        let attrs = Attr::parse(attrs_text)?
            .into_dict()
            .ok_or_else(|| IrError::Parse(format!("attrs must be dict: {input}")))?;
        Ok((ty, attrs))
    } else {
        Ok((input.trim(), AttrMap::new()))
    }
}

fn parse_type_use(input: &str) -> Result<TypeUse, IrError> {
    Ok(TypeUse { ty: TypeExpr::parse(input.trim())? })
}

fn looks_like_operation_line(line: &str) -> bool {
    line.split_once(" = ")
        .map(|(lhs, rest)| lhs.trim().starts_with('%') && rest.trim().starts_with('"'))
        .unwrap_or(false)
}

fn parse_value_ref(input: &str) -> Result<&str, IrError> {
    let name = input.trim().strip_prefix('%').ok_or_else(|| {
        IrError::Parse(format!("value reference must start with %: {input}"))
    })?;
    if name.is_empty() {
        return Err(IrError::Parse(format!("empty value reference: {input}")));
    }
    Ok(name)
}

fn parse_quoted_name<'a>(
    input: &'a str,
    line: &str,
) -> Result<(String, &'a str), IrError> {
    let input = input.trim();
    let rest = input.strip_prefix('"').ok_or_else(|| {
        IrError::Parse(format!("operation name must be quoted: {line}"))
    })?;
    let close = rest.find('"').ok_or_else(|| {
        IrError::Parse(format!("operation name missing quote: {line}"))
    })?;
    Ok((rest[..close].to_string(), &rest[close + 1..]))
}

fn find_matching_delimiter(
    input: &str,
    open_index: usize,
    open_char: char,
    close_char: char,
) -> Result<usize, IrError> {
    let mut depth = 0usize;
    let mut in_string = false;
    for (index, ch) in input.char_indices().skip_while(|(index, _)| *index < open_index)
    {
        if ch == '"' {
            in_string = !in_string;
            continue;
        }
        if in_string {
            continue;
        }
        if ch == open_char {
            depth += 1;
        } else if ch == close_char {
            depth = depth.saturating_sub(1);
            if depth == 0 {
                return Ok(index);
            }
        }
    }
    Err(IrError::Parse(format!("missing '{close_char}' in {input}")))
}

fn parse_function_type(input: &str) -> Result<(Vec<TypeUse>, Vec<TypeUse>), IrError> {
    let input = input.trim();
    let open = input.find('(').ok_or_else(|| {
        IrError::Parse(format!("function type missing inputs: {input}"))
    })?;
    if open != 0 {
        return Err(IrError::Parse(format!(
            "function type must start with '(': {input}"
        )));
    }
    let close = find_matching_delimiter(input, open, '(', ')')?;
    let operands = split_top_level(&input[open + 1..close], ',')
        .into_iter()
        .filter(|ty| !ty.trim().is_empty())
        .map(|ty| parse_type_use(ty.trim()))
        .collect::<Result<Vec<_>, _>>()?;
    let result_text = input[close + 1..]
        .trim()
        .strip_prefix("->")
        .ok_or_else(|| {
            IrError::Parse(format!("function type missing results: {input}"))
        })?
        .trim();
    Ok((operands, parse_result_type_uses(result_text)?))
}

fn parse_result_type_uses(input: &str) -> Result<Vec<TypeUse>, IrError> {
    let input = input.trim();
    if input.is_empty() {
        return Ok(Vec::new());
    }
    let body = if input.starts_with('(') {
        if !input.ends_with(')') {
            return Err(IrError::Parse(format!(
                "result type list missing ')': {input}"
            )));
        }
        &input[1..input.len() - 1]
    } else {
        input
    };
    split_top_level(body, ',')
        .into_iter()
        .filter(|ty| !ty.trim().is_empty())
        .map(|ty| parse_type_use(ty.trim()))
        .collect()
}

fn format_result_type_list(types: &[String]) -> String {
    match types {
        [] => "()".to_string(),
        [one] => one.clone(),
        many => format!("({})", many.join(", ")),
    }
}

fn parse_return_line(
    graph: &Graph,
    values: &BTreeMap<String, Value>,
    expected_outputs: &[BlockOutputDecl],
    line: &str,
) -> Result<Vec<Value>, IrError> {
    let rest = line
        .strip_prefix("return ")
        .ok_or_else(|| IrError::Parse(format!("invalid return: {line}")))?;
    let (value_text, type_text) = rest
        .split_once(':')
        .ok_or_else(|| IrError::Parse(format!("return missing type list: {line}")))?;
    let returned = split_top_level(value_text, ',')
        .into_iter()
        .filter(|name| !name.trim().is_empty())
        .map(|name| {
            let name = parse_value_ref(name.trim())?;
            values
                .get(name)
                .copied()
                .ok_or_else(|| IrError::Parse(format!("unknown return value {name}")))
        })
        .collect::<Result<Vec<_>, _>>()?;
    let returned_types = parse_result_type_uses(type_text.trim())?;
    if returned.len() != returned_types.len() {
        return Err(IrError::Parse(format!(
            "return value/type count mismatch: {line}"
        )));
    }
    if !expected_outputs.is_empty() && expected_outputs.len() != returned.len() {
        return Err(IrError::Parse(format!(
            "return count does not match function type: {line}"
        )));
    }
    for (index, value) in returned.iter().enumerate() {
        let actual = graph.type_expr(graph.type_of_value(*value)?)?;
        let declared = &returned_types[index];
        if actual != &declared.ty {
            return Err(IrError::Parse(format!(
                "return value {index} has type {actual}, declared {}",
                declared.ty
            )));
        }
        if let Some(expected) = expected_outputs.get(index) {
            if declared.ty != expected.ty {
                return Err(IrError::Parse(format!(
                    "return value {index} has type {}, expected {}",
                    declared.ty, expected.ty
                )));
            }
        }
    }
    Ok(returned)
}
