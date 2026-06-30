cmake_minimum_required(VERSION 3.20)
project(kvm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

# The whole project is one small program. We don't publish libraries; we just
# compile every source once into an OBJECT library so the `kvm` binary and the
# test executables can share the objects without recompiling them per target.
add_library(kvm_obj OBJECT
  src/ir/graph.cpp
  src/ir/serialization/serializer.cpp
  src/ir/serialization/parser.cpp
  src/ir/visualization/visualizer.cpp
  src/ir/dialect/kernel/values.cpp
  src/ir/dialect/kernel/ops.cpp
  src/ir/dialect/kernel/dialect.cpp
  src/ir/dialect/task/ops.cpp
  src/ir/dialect/task/dialect.cpp
  src/ir/dialect/tile/ops.cpp
  src/ir/dialect/tile/verify.cpp
  src/ir/dialect/tile/dialect.cpp
  src/device/device.cpp
  src/transform/kernel_to_task/kernel_to_task.cpp
)
# IR headers are included relative to the ir root, e.g. "graph.h",
# "dialect/kernel/values.h", "serialization/registry.h". device/ and transform/
# sit beside ir/ and include each other / the IR as "device/...",
# "transform/...", so src/ is also a root.
target_include_directories(kvm_obj PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ir
)

# glog for logging (VLOG) and assertions (CHECK). Found via the system install
# (e.g. Homebrew); not vendored. PUBLIC so the kvm binary and tests linking
# kvm_obj also get it.
find_package(glog REQUIRED)
target_link_libraries(kvm_obj PUBLIC glog::glog)

# CLI11 (header-only) is vendored as a git submodule under third_party/CLI11.
set(CLI11_DIR ${CMAKE_SOURCE_DIR}/third_party/CLI11)
if(NOT EXISTS ${CLI11_DIR}/include/CLI/CLI.hpp)
  message(FATAL_ERROR
    "third_party/CLI11 is empty. Run: git submodule update --init --recursive")
endif()

# The single `kvm` binary. Dropped in build/bin/ (a fixed, easy-to-find spot
# inside the build dir) rather than mirrored deep under build/src/cli/.
add_executable(kvm src/cli/main.cpp src/cli/dialects.cpp)
target_include_directories(kvm PRIVATE ${CLI11_DIR}/include)
target_link_libraries(kvm PRIVATE kvm_obj)
set_target_properties(kvm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

enable_testing()
add_subdirectory(tests)
