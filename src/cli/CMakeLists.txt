# The `kvm` CLI. Argument parsing uses CLI11, vendored as a git submodule under
# third_party/CLI11 (header-only; no system install, no hand-rolled parser).
set(CLI11_DIR ${CMAKE_SOURCE_DIR}/third_party/CLI11)
if(NOT EXISTS ${CLI11_DIR}/include/CLI/CLI.hpp)
  message(FATAL_ERROR
    "third_party/CLI11 is empty. Run: git submodule update --init --recursive")
endif()

add_executable(kvm main.cpp)
target_include_directories(kvm PRIVATE ${CLI11_DIR}/include)
target_link_libraries(kvm PRIVATE kvm_ir)
