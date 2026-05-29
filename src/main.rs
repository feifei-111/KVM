use std::env;
use std::fs;
use std::process::ExitCode;

use kvm::compiler::dialects::{
    DialectDescription, OperationDescription, ValueTypeDescription, all_descriptions,
    find_description,
};
use kvm::compiler::ir::Graph;
use kvm::render::{GraphRenderOptions, write_dot, write_html, write_png};

fn main() -> ExitCode {
    match run(env::args().skip(1).collect()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(message) => {
            eprintln!("{message}");
            ExitCode::FAILURE
        }
    }
}

fn run(args: Vec<String>) -> Result<(), String> {
    if args
        .iter()
        .any(|arg| matches!(arg.as_str(), "--ir" | "--dot" | "--png" | "--html"))
    {
        return load_ir(IrOptions::parse(&args)?);
    }

    match args.as_slice() {
        [] => {
            print_help();
            Ok(())
        }
        [cmd] if cmd == "help" || cmd == "--help" || cmd == "-h" => {
            print_help();
            Ok(())
        }
        [cmd] if cmd == "dialect" => {
            print_dialect_help();
            Ok(())
        }
        [cmd, subcmd] if cmd == "dialect" && subcmd == "list" => {
            for dialect in all_descriptions() {
                println!("{}", dialect.name);
            }
            Ok(())
        }
        [cmd, subcmd, name] if cmd == "dialect" && subcmd == "show" => {
            let description = find_description(name)
                .ok_or_else(|| format!("unknown dialect: {name}"))?;
            print_dialect(description);
            Ok(())
        }
        _ => Err("unknown command; run `kvm help`".to_string()),
    }
}

fn print_help() {
    println!("kvm");
    println!();
    println!("Commands:");
    println!("  dialect list          List available dialects");
    println!("  dialect show <name>   Show dialect values and operations");
    println!();
    println!("IR:");
    println!("  --ir <path> [--html <path>] [--png <path>] [--dot <path>]");
}

fn print_dialect_help() {
    println!("kvm dialect");
    println!();
    println!("Commands:");
    println!("  list          List available dialects");
    println!("  show <name>   Show dialect values and operations");
}

fn load_ir(options: IrOptions) -> Result<(), String> {
    let path =
        options.ir.as_deref().ok_or_else(|| "--ir requires a path".to_string())?;
    let text = fs::read_to_string(path)
        .map_err(|err| format!("failed to read {path}: {err}"))?;
    let graph = Graph::parse_text(&text)
        .map_err(|err| format!("failed to parse {path}: {err}"))?;
    graph.verify().map_err(|err| format!("failed to verify {path}: {err}"))?;

    if options.dot.is_none() && options.png.is_none() && options.html.is_none() {
        return Ok(());
    }

    let render_options = GraphRenderOptions { show_types: true, show_attrs: true };
    if let Some(dot_path) = &options.dot {
        write_dot(&graph, dot_path, render_options)?;
        println!("wrote dot: {dot_path}");
    }
    if let Some(png_path) = &options.png {
        write_png(&graph, png_path, render_options)?;
        println!("wrote png: {png_path}");
    }
    if let Some(html_path) = &options.html {
        write_html(&graph, html_path, render_options)?;
        println!("wrote html: {html_path}");
    }

    Ok(())
}

#[derive(Debug, Default)]
struct IrOptions {
    ir: Option<String>,
    dot: Option<String>,
    png: Option<String>,
    html: Option<String>,
}

impl IrOptions {
    fn parse(args: &[String]) -> Result<Self, String> {
        let mut options = Self::default();
        let mut index = 0;
        while index < args.len() {
            match args[index].as_str() {
                "--ir" => {
                    index += 1;
                    options.ir = Some(
                        args.get(index)
                            .ok_or_else(|| "--ir requires a path".to_string())?
                            .clone(),
                    );
                }
                "--dot" => {
                    index += 1;
                    options.dot = Some(
                        args.get(index)
                            .ok_or_else(|| "--dot requires a path".to_string())?
                            .clone(),
                    );
                }
                "--png" => {
                    index += 1;
                    options.png = Some(
                        args.get(index)
                            .ok_or_else(|| "--png requires a path".to_string())?
                            .clone(),
                    );
                }
                "--html" => {
                    index += 1;
                    options.html = Some(
                        args.get(index)
                            .ok_or_else(|| "--html requires a path".to_string())?
                            .clone(),
                    );
                }
                flag => return Err(format!("unknown option: {flag}")),
            }
            index += 1;
        }
        Ok(options)
    }
}

fn print_dialect(description: DialectDescription) {
    println!("{}", description.name.to_uppercase());
    println!();
    if !description.graph_properties.is_empty() {
        println!("graph");
        for property in description.graph_properties {
            println!("  {} : {}", property.name, property.ty);
        }
        println!();
    }
    println!("values");
    for value_type in description.value_types {
        print_value_type(value_type);
    }
    println!();
    println!("operations");
    let width = description
        .operations
        .iter()
        .map(|operation| operation_label(description.name, operation).len())
        .max()
        .unwrap_or(0);
    for operation in description.operations {
        print_operation(description.name, operation, width);
    }
}

fn print_value_type(value_type: &ValueTypeDescription) {
    if value_type.attrs.is_empty() {
        println!("  {}", value_type.name);
    } else {
        let attrs = value_type
            .attrs
            .iter()
            .map(|attr| format!("{}: {}", attr.name, attr.ty))
            .collect::<Vec<_>>()
            .join(", ");
        println!("  {}<{}>", value_type.name, attrs);
    }
}

fn print_operation(dialect: &str, operation: &OperationDescription, width: usize) {
    let name = operation_label(dialect, operation);
    println!(
        "  {name:<width$} : {} -> {}",
        format_args(operation.operands),
        format_args(operation.results),
        width = width
    );
}

fn format_args(args: &[kvm::compiler::dialects::ArgumentDescription]) -> String {
    if args.is_empty() {
        "()".to_string()
    } else {
        args.iter().map(|arg| arg.ty).collect::<Vec<_>>().join(", ")
    }
}

fn display_operation_name<'a>(dialect: &str, name: &'a str) -> &'a str {
    name.strip_prefix(&format!("{dialect}.")).unwrap_or(name)
}

fn operation_label(dialect: &str, operation: &OperationDescription) -> String {
    let name = display_operation_name(dialect, operation.name);
    if operation.attrs.is_empty() {
        return name.to_string();
    }
    let attrs = operation
        .attrs
        .iter()
        .map(|attr| format!("{}: {}", attr.name, attr.ty))
        .collect::<Vec<_>>()
        .join(", ");
    format!("{name} {{ {attrs} }}")
}
