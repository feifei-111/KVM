#pragma once

// Result of an operator's verify hook.
//
// verify is a dialect-local concern: each dialect exposes a Verify(op, ins,
// outs) that dispatches on the op name to per-op checks. It is NOT a registry /
// runtime table -- the op set of a dialect is known at compile time, so the
// dispatch is just a switch in that dialect's code. This struct is the one
// shared piece (the return type), so it lives here rather than in any single
// dialect.
//
// ok == true means the operation's inputs/outputs satisfy the op's fixed rules
// (operand count, dtype, location, shape relations). On failure, `error` holds
// a human-readable reason. Rules that would depend on the device are NOT here:
// if an op's legality differs by device, that is a different op (see Notion
// "task graph" naming rule), not a device parameter to verify.

#include <string>
#include <utility>

namespace kvm::ir {

struct VerifyResult {
  bool ok = true;
  std::string error;

  static VerifyResult Ok() { return {}; }
  static VerifyResult Fail(std::string msg) { return {false, std::move(msg)}; }
};

}  // namespace kvm::ir
