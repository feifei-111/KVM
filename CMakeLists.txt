cmake_minimum_required(VERSION 3.20)
project(kvm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

add_subdirectory(src/ir)
add_subdirectory(src/dialect/kernel)
add_subdirectory(src/cli)

enable_testing()
add_subdirectory(tests)
