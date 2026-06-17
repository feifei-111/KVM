# Lightweight tests: each file is a standalone executable registered with
# CTest. No third-party framework (see test_harness.h).
add_executable(test_value test_value.cpp)
target_link_libraries(test_value PRIVATE kvm_ir)

add_executable(test_graph test_graph.cpp)
target_link_libraries(test_graph PRIVATE kvm_ir)

add_executable(test_attr test_attr.cpp)
target_link_libraries(test_attr PRIVATE kvm_ir)

add_executable(test_serialize test_serialize.cpp)
target_link_libraries(test_serialize PRIVATE kvm_ir)

add_executable(test_visualize test_visualize.cpp)
target_link_libraries(test_visualize PRIVATE kvm_ir)

add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE kvm_ir)

add_test(NAME value COMMAND test_value)
add_test(NAME graph COMMAND test_graph)
add_test(NAME attr COMMAND test_attr)
add_test(NAME serialize COMMAND test_serialize)
add_test(NAME visualize COMMAND test_visualize)
add_test(NAME parse COMMAND test_parse)
