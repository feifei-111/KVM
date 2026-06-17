#pragma once

// Tiny test harness: no third-party dependency, just an assert macro that
// reports file:line and a message, and a main() that runs registered cases.
// Each test file links one CASE list and is registered with CTest.

#include <cstdio>
#include <string>
#include <vector>

namespace kvmtest {

struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& Registry() {
  static std::vector<Case> cases;
  return cases;
}

inline int& Failures() {
  static int n = 0;
  return n;
}

struct Register {
  Register(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

}  // namespace kvmtest

#define KVM_TEST(name)                                     \
  static void name();                                      \
  static ::kvmtest::Register kvm_reg_##name(#name, &name); \
  static void name()

#define KVM_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("  FAIL %s:%d: KVM_CHECK(%s)\n", __FILE__, __LINE__, #cond); \
      ++::kvmtest::Failures();                                                 \
    }                                                                          \
  } while (0)

#define KVM_RUN_ALL()                                                         \
  int main() {                                                                \
    for (const auto& c : ::kvmtest::Registry()) {                             \
      int before = ::kvmtest::Failures();                                     \
      c.fn();                                                                 \
      std::printf("[%s] %s\n",                                                \
                  ::kvmtest::Failures() == before ? "PASS" : "FAIL", c.name); \
    }                                                                         \
    if (::kvmtest::Failures() == 0) {                                         \
      std::printf("all tests passed\n");                                      \
      return 0;                                                               \
    }                                                                         \
    std::printf("%d check(s) failed\n", ::kvmtest::Failures());               \
    return 1;                                                                 \
  }
