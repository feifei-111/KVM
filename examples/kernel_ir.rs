use kvm::compiler::dialects::kernel::{DType, KernelBuilder, Layout, TensorType};
use kvm::compiler::ir::{Attr, AttrMap, Graph, RenderFormat, RenderOptions};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut graph = Graph::new();
    let mut kernel = KernelBuilder::new(&mut graph);

    let tensor = kernel.tensor_type(TensorType::new(
        DType::F16,
        [128usize, 128usize],
        Layout::RowMajor,
    ));
    let lhs = kernel.input("lhs", tensor)?;
    let rhs = kernel.input("rhs", tensor)?;
    let out = kernel.matmul_with_attrs(
        lhs,
        rhs,
        tensor,
        DType::F32,
        AttrMap::from([(
            "debug.name".to_string(),
            Attr::String("qk_matmul".to_string()),
        )]),
    )?;
    graph.set_value_name(out, "out")?;

    println!("-- serialized --");
    println!("{}", graph.to_text()?);
    println!();

    println!("-- text graph --");
    println!(
        "{}",
        graph.render(
            RenderFormat::Text,
            RenderOptions { show_types: true, show_attrs: true },
        )?
    );
    println!();

    println!("-- dot graph --");
    println!(
        "{}",
        graph.render(
            RenderFormat::Dot,
            RenderOptions { show_types: true, show_attrs: false },
        )?
    );

    Ok(())
}
