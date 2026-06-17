// kvm command-line tool.
//
// Current capability: read a .kvm text file and emit an HTML visualization.
//   kvm -i graph.kvm -o graph.html

#include <CLI/CLI.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "attr.h"
#include "graph.h"
#include "serialization/builtin_codecs.h"
#include "serialization/parser.h"
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

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"kvm -- KVM IR command-line tool"};
  app.set_version_flag("--version", "kvm 0.0.0");

  std::string input;
  std::string output;
  app.add_option("-i,--input", input, "Input .kvm IR text file")
      ->required()
      ->check(CLI::ExistingFile);
  app.add_option("-o,--output", output, "Output .html visualization file")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    // Builtins are registered for impl codecs; a real front-end would also
    // register its dialects' operators here so the parser can look them up.
    kvm::ir::serial::Registry registry;
    kvm::ir::serial::RegisterBuiltins(registry);

    kvm::ir::Graph graph;
    kvm::ir::AttrMap attrs =
        kvm::ir::serial::Parse(ReadFile(input), registry, graph);

    std::string html = kvm::ir::viz::RenderHtml(graph, &attrs);
    WriteFile(output, html);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "kvm: %s\n", e.what());
    return 1;
  }
  return 0;
}
