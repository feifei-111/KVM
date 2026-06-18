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
  src/dialect/kernel/attn.cpp
  src/dialect/kernel/dist.cpp
  src/dialect/kernel/dialect.cpp
)
# Headers are included both as "graph.h" (within ir) and "dialect/kernel/..".
target_include_directories(kvm_obj PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ir
)

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
