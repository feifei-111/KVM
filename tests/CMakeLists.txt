# Lightweight tests: each file is a standalone executable registered with
# CTest. No third-party framework (see test_harness.h).
add_executable(test_value test_value.cpp)
target_link_libraries(test_value PRIVATE kvm_ir)

add_executable(test_graph test_graph.cpp)
target_link_libraries(test_graph PRIVATE kvm_ir)

add_executable(test_attr test_attr.cpp)
target_link_libraries(test_attr PRIVATE kvm_ir)

add_test(NAME value COMMAND test_value)
add_test(NAME graph COMMAND test_graph)
add_test(NAME attr COMMAND test_attr)
