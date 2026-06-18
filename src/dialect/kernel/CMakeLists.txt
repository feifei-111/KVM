add_library(kvm_dialect_kernel
  attn.cpp
  dist.cpp
  dialect.cpp
)

target_include_directories(kvm_dialect_kernel PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kvm_dialect_kernel PUBLIC kvm_ir)
