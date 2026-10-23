cmake_minimum_required(VERSION 3.24)
project(ExeCon VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS" FORCE)
  if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    message(STATUS "Non-arm64 host: portable components only")
  endif()
endif()
option(EXECON_SANITIZE "Enable ASan and UBSan (interpreter only)" OFF)
option(EXECON_BUILD_TESTS "Build developer tests and benchmark" ON)
option(EXECON_FUZZ "Build libFuzzer PE target (requires compiler runtime)" OFF)
add_compile_options(-Wall -Wextra -Werror)
if(EXECON_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
  add_compile_definitions(EXECON_SANITIZE=1)
endif()
add_library(execon_engine STATIC
  engine/api/execon_engine.cpp
  engine/cpu/context.cpp
  engine/host/jit_memory.cpp engine/host/log.cpp engine/host/process.cpp
  engine/ir/ir.cpp
  engine/jit/backend.cpp engine/jit/encoder.cpp
  engine/mem/address_space.cpp
  engine/nt/heap.cpp engine/nt/paths.cpp engine/nt/services.cpp engine/nt/text.cpp
  engine/pe/image.cpp
  engine/win32/modules.cpp
  engine/x86/decoder.cpp
)
target_include_directories(execon_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(execon cli/main.cpp)
target_link_libraries(execon PRIVATE execon_engine)
install(TARGETS execon RUNTIME DESTINATION bin)
if(EXECON_BUILD_TESTS)
  enable_testing()
  add_executable(execon_tests tests/unit/main.cpp tests/unit/pe.cpp tests/unit/cpu.cpp
    tests/unit/jit.cpp tests/unit/runtime.cpp)
  target_link_libraries(execon_tests PRIVATE execon_engine)
  add_test(NAME unit COMMAND execon_tests)
  add_executable(execon_diff tests/diff/runner.cpp)
  target_link_libraries(execon_diff PRIVATE execon_engine)
  add_executable(execon_api_test tests/integration/api.cpp)
  target_link_libraries(execon_api_test PRIVATE execon_engine)
  add_executable(execon_bench tests/bench/main.cpp)
  target_link_libraries(execon_bench PRIVATE execon_engine)
endif()
if(EXECON_FUZZ)
  target_compile_options(execon_engine PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
  target_link_options(execon_engine INTERFACE -fsanitize=fuzzer-no-link,address,undefined)
  add_executable(execon_fuzz_pe tests/unit/fuzz_pe.cpp)
  target_link_libraries(execon_fuzz_pe PRIVATE execon_engine)
  target_compile_options(execon_fuzz_pe PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(execon_fuzz_pe PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
