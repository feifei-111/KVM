#include "transform/kernel_to_task/kernel_to_task.h"

#include <glog/logging.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "device/device.h"
#include "dialect/kernel/values.h"
#include "dialect/task/values.h"
#include "node.h"

namespace kvm::transform {
namespace {

namespace kernel = ir::kernel;
namespace task = ir::task;
using ir::Block;
using ir::Graph;
using ir::OpNode;
using ir::Value;
using ir::ValueNode;

// A kernel block argument becomes a task value in global memory: the algebraic
// tensor gains a storage location (its decomp results will move it around).
ValueNode* TranslateInput(const ValueNode* kv, Block* tblock) {
  const Value& v = kv->value();
  const kernel::TensorImpl* t = v.impl.as<kernel::TensorImpl>();
  CHECK(t) << "decomp: block input %" << v.name
           << " is not a kernel tensor (only tensors are lowered for now)";
  return tblock->AddArgument(
      Value{v.name,
            {"tensor", "task"},
            task::TensorImpl{t->shape, t->stride, t->dtype,
                             task::Location::kGlobal}});
}

}  // namespace

void TransformKernelToTask(const ir::Graph& kernel, ir::Graph& task) {
  CHECK(kernel.root()) << "kernel_to_task: kernel graph has no root block";
  task.config() = kernel.config();

  // Device is fully determined by the graph's config; build it here rather than
  // taking one from the caller (which could disagree with the graph).
  device::Device device(kernel.config());

  Block* tblock = task.MakeRoot(kernel.root()->name());

  // kernel ValueNode* -> task ValueNode*. Seeded by block arguments; extended
  // as each op's task outputs are produced.
  std::unordered_map<const ValueNode*, ValueNode*> map;

  const Block* kblock = kernel.root();
  for (const ValueNode* arg : kblock->arguments()) {
    map[arg] = TranslateInput(arg, tblock);
  }

  auto resolve = [&](const ValueNode* kv) -> ValueNode* {
    auto it = map.find(kv);
    CHECK(it != map.end())
        << "decomp: operand %" << kv->value().name
        << " has no task value (not in SSA order, or produced by a skipped op)";
    return it->second;
  };

  for (const OpNode* kop : kblock->operations()) {
    // Resolve operands to their task values, and collect the kernel operand
    // names for the subgraph naming scheme.
    std::vector<ValueNode*> task_ins;
    std::vector<std::string> in_names;
    task_ins.reserve(kop->operands().size());
    in_names.reserve(kop->operands().size());
    for (const ValueNode* kin : kop->operands()) {
      task_ins.push_back(resolve(kin));
      in_names.push_back(kin->value().name);
    }

    std::vector<const ValueNode*> kouts(kop->results().begin(),
                                        kop->results().end());
    std::vector<ValueNode*> task_outs =
        device.Decomp(kop, task_ins, in_names, kouts, tblock);
    CHECK_EQ(task_outs.size(), kouts.size())
        << "decomp: op '" << kop->op().op.name << "' produced "
        << task_outs.size() << " outputs, expected " << kouts.size();
    for (std::size_t i = 0; i < kouts.size(); ++i) map[kouts[i]] = task_outs[i];
  }

  std::vector<ValueNode*> outs;
  outs.reserve(kblock->outputs().size());
  for (const ValueNode* kout : kblock->outputs()) outs.push_back(resolve(kout));
  if (!outs.empty()) tblock->SetOutputs(std::move(outs));
}

}  // namespace kvm::transform
