# Lightweight tests: each file is a standalone executable registered with
# CTest. No third-party framework (see test_harness.h). All reuse the shared
# objects in kvm_obj rather than recompiling sources.
foreach(t value graph attr serialize visualize parse kernel_dialect)
  add_executable(test_${t} test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE kvm_obj)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()
