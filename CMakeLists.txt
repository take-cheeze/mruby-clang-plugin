cmake_minimum_required(VERSION 2.8.7)

if(NOT DEFINED LLVM_CONFIG)
  set(LLVM_CONFIG "llvm-config")
endif()
execute_process(
  COMMAND "${LLVM_CONFIG}" "--version"
  OUTPUT_VARIABLE llvm_version
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot use llvm-config: ${LLVM_CONFIG}")
endif()
message(STATUS "LLVM version: ${llvm_version}")
message(STATUS "Using llvm-config: ${LLVM_CONFIG}")

if(EXISTS "${MRUBY_ROOT}")
  if(IS_ABSOLUTE "${MRUBY_ROOT}")
    set(MRUBY_ROOT "${MRUBY_ROOT}")
  else()
    get_filename_component(MRUBY_ROOT "${MRUBY_ROOT}" ABSOLUTE)
  endif()
endif()

if(NOT DEFINED MRUBY_ROOT)
  message(FATAL_ERROR "cannot find mruby")
endif()

execute_process(
  COMMAND "${LLVM_CONFIG}" "--includedir"
  OUTPUT_VARIABLE llvm_include_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE)
include_directories("${llvm_include_dir}")

execute_process(
  COMMAND "${LLVM_CONFIG}" "--ldflags"
  OUTPUT_VARIABLE llvm_ldflags
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_SHARED_LINKER_FLAGS ${llvm_ldflags})
if(APPLE)
  set(CMAKE_MODULE_LINKER_FLAGS "${llvm_ldflags} -undefined dynamic_lookup")
endif()

execute_process(
  COMMAND "${LLVM_CONFIG}" "--cxxflags"
  OUTPUT_VARIABLE llvm_cxxflags
  OUTPUT_STRIP_TRAILING_WHITESPACE)
list(APPEND CMAKE_CXX_FLAGS "${llvm_cxxflags}")
if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/src" SRCS)
set(library_name "mruby-clang-checker")
add_library("${library_name}" MODULE "${SRCS}")

set(plugin_file $<TARGET_FILE:${library_name}>)

if(NOT DEFINED TEST_CLANG)
  execute_process(
    COMMAND "${LLVM_CONFIG}" "--bindir"
    OUTPUT_VARIABLE llvm_bin_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(TEST_CLANG "${llvm_bin_dir}/clang")
endif()

enable_testing()
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/test" TESTS)
foreach(i ${TESTS})
  get_filename_component(test_name "${i}" NAME)
  add_test(
    NAME "${test_name}"
    COMMAND
    "${TEST_CLANG}"
    -Xclang -load -Xclang "${plugin_file}" -Xclang -add-plugin -Xclang "${library_name}"
    -ferror-limit=0
    -fsyntax-only -Xclang -verify
    "-I${MRUBY_ROOT}/include" "${i}")
endforeach()
