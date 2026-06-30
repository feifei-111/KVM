// Tests for the HTML/DOT visualizer.
#include <cstdlib>
#include <string>

#include "attr.h"
#include "graph.h"
#include "test_harness.h"
#include "visualization/visualizer.h"

using namespace kvm::ir;

namespace {
Type T() { return {"tensor", "hlo"}; }
Operator Add() { return {"add", "hlo", {T(), T()}, {T()}}; }

bool HasDot() { return std::system("command -v dot >/dev/null 2>&1") == 0; }
}  // namespace

KVM_TEST(dot_has_value_and_op_nodes_and_edges) {
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  ValueNode* b = root->AddArgument(Value{"b", T(), {}});
  OpNode* op =
      root->MakeOperation(Operation{Add(), {}}, {a, b}, {Value{"c", T(), {}}});
  root->SetOutputs({op->results()[0]});

  std::string dot = viz::RenderDot(g);
  KVM_CHECK(dot.find("digraph ir") != std::string::npos);
  // value and op nodes
  KVM_CHECK(dot.find("[label=\"%a\"") != std::string::npos);
  KVM_CHECK(dot.find("[label=\"hlo.add\"") != std::string::npos);
  // an operand edge value -> op and a result edge op -> value exist
  KVM_CHECK(dot.find("-> op0;") != std::string::npos);
  KVM_CHECK(dot.find("op0 -> value") != std::string::npos);
}

KVM_TEST(html_embeds_svg_and_details_when_dot_available) {
  if (!HasDot()) {
    std::printf("  (skipped: graphviz `dot` not on PATH)\n");
    return;
  }
  Graph g;
  Block* root = g.MakeRoot();
  ValueNode* a = root->AddArgument(Value{"a", T(), {}});
  OpNode* op =
      root->MakeOperation(Operation{Add(), {}}, {a, a}, {Value{"c", T(), {}}});
  root->SetOutputs({op->results()[0]});

  AttrMap attrs;
  attrs.Set(a, "rank", 3);

  std::string html = viz::RenderHtml(g, &attrs);
  KVM_CHECK(html.find("<!doctype html>") != std::string::npos);
  KVM_CHECK(html.find("<svg") != std::string::npos);  // embedded SVG
  KVM_CHECK(html.find("const details =") !=
            std::string::npos);  // inspector data
  KVM_CHECK(html.find("KVM IR Graph") != std::string::npos);
}

KVM_RUN_ALL()
