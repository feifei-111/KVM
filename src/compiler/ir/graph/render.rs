use super::{Graph, RenderFormat, RenderOptions};
use crate::compiler::ir::IrError;
use crate::compiler::ir::attr::format_attr_map;

impl Graph {
    pub fn render(
        &self,
        format: RenderFormat,
        options: RenderOptions,
    ) -> Result<String, IrError> {
        match format {
            RenderFormat::Text => self.render_text(options),
        }
    }

    fn render_text(&self, options: RenderOptions) -> Result<String, IrError> {
        let names = self.value_names();
        let mut out = String::new();
        if options.show_attrs && !self.properties.is_empty() {
            out.push_str("graph");
            out.push('\n');
            out.push_str(&format!("  {}\n", format_attr_map(&self.properties)));
        }
        for op in self.topological_operations()? {
            let data = &self.operations[op.index()];
            let op_name =
                data.name.clone().unwrap_or_else(|| format!("operation{}", op.index()));
            out.push_str(&op_name);
            if options.show_types {
                out.push_str(&format!(" [{}]", self.type_expr(data.ty)?));
            }
            out.push('\n');
            for operand in &data.operands {
                out.push_str(&format!("  <- {}\n", names[operand.index()]));
            }
            for result in &data.results {
                out.push_str(&format!("  -> {}\n", names[result.index()]));
            }
            if options.show_attrs && !data.attrs.is_empty() {
                out.push_str(&format!("  {}\n", format_attr_map(&data.attrs)));
            }
        }
        Ok(out.trim_end().to_string())
    }
}
