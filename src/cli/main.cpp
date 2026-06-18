// kvm command-line tool.
//
//   kvm -i graph.kvm -o graph.html   read IR text, write an HTML visualization
//   kvm show dialect                 list available dialects
//   kvm show d:<name>                list a dialect's registered ops + impls

#include <CLI/CLI.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "attr.h"
#include "dialects.h"
#include "graph.h"
#include "serialization/parser.h"
#include "serialization/registry.h"
#include "visualization/visualizer.h"

namespace {

std::string ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open input: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot open output: " + path);
  out << content;
}

std::string TypeText(const kvm::ir::Type& t) {
  return t.dialect.empty() ? t.name : t.dialect + "." + t.name;
}

// `kvm -i .kvm -o .html`
int RunVisualize(const std::string& input, const std::string& output) {
  kvm::ir::serial::Registry registry;
  kvm::cli::RegisterAllDialects(registry);

  kvm::ir::Graph graph;
  kvm::ir::AttrMap attrs =
      kvm::ir::serial::Parse(ReadFile(input), registry, graph);
  WriteFile(output, kvm::ir::viz::RenderHtml(graph, &attrs));
  return 0;
}

// `kvm show dialect`
int ShowDialects() {
  std::printf("dialects:\n");
  for (const auto& d : kvm::cli::Dialects()) {
    std::printf("  %s\n", d.name.c_str());
  }
  return 0;
}

// `kvm show d:<name>`
int ShowDialect(const std::string& name) {
  const kvm::cli::DialectEntry* found = nullptr;
  for (const auto& d : kvm::cli::Dialects()) {
    if (d.name == name) found = &d;
  }
  if (!found) {
    std::fprintf(stderr, "kvm: unknown dialect '%s'\n", name.c_str());
    return 1;
  }

  kvm::ir::serial::Registry r;
  found->register_into(r);

  std::printf("dialect %s\n\noperators:\n", name.c_str());
  for (const auto& [key, op] : r.Operators()) {
    std::string sig;
    for (std::size_t i = 0; i < op.inputs.size(); ++i) {
      if (i) sig += ", ";
      sig += TypeText(op.inputs[i]);
    }
    sig += " -> ";
    for (std::size_t i = 0; i < op.outputs.size(); ++i) {
      if (i) sig += ", ";
      sig += TypeText(op.outputs[i]);
    }
    std::printf("  %-26s :: %s\n", key.c_str(), sig.c_str());
  }

  std::printf("\nvalue impls:\n");
  for (const auto& k : r.ValueImplKeys()) std::printf("  %s\n", k.c_str());
  std::printf("\noperation impls:\n");
  for (const auto& k : r.OperationImplKeys()) std::printf("  %s\n", k.c_str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"kvm -- KVM IR command-line tool"};
  app.set_version_flag("--version", "kvm 0.0.0");
  app.require_subcommand(0, 1);

  // top-level visualize options (kvm -i x.kvm -o x.html)
  std::string input;
  std::string output;
  app.add_option("-i,--input", input, "Input .kvm IR text file")
      ->check(CLI::ExistingFile);
  app.add_option("-o,--output", output, "Output .html visualization file");

  // kvm show <target>
  auto* show = app.add_subcommand("show", "Inspect registered IR information");
  std::string target;
  show->add_option("target", target,
                   "What to show: 'dialect' (list all) or 'd:<name>'")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    if (show->parsed()) {
      if (target == "dialect") return ShowDialects();
      if (target.rfind("d:", 0) == 0) return ShowDialect(target.substr(2));
      std::fprintf(
          stderr,
          "kvm: unknown show target '%s' (use 'dialect' or 'd:<name>')\n",
          target.c_str());
      return 1;
    }

    // default action: visualize, requires -i and -o
    if (input.empty() || output.empty()) {
      std::fprintf(stderr, "%s", app.help().c_str());
      return 1;
    }
    return RunVisualize(input, output);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "kvm: %s\n", e.what());
    return 1;
  }
}
