add_library(kvm_ir
  graph.cpp
  serialization/serializer.cpp
)

target_include_directories(kvm_ir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
