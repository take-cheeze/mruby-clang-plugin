cmake_minimum_required(VERSION 2.8)

if(EXISTS "$ENV{MRUBY_ROOT}")
  if(IS_ABSOLUTE "$ENV{MRUBY_ROOT}")
    set(MRUBY_ROOT "$ENV{MRUBY_ROOT}")
  else()
    get_filename_component(MRUBY_ROOT "$ENV{MRUBY_ROOT}" ABSOLUTE)
  endif()
  set(MRUBY_ROOT "${MRUBY_ROOT}" CACHE PATH "mruby root path used in test")
endif()

execute_process(
  COMMAND "llvm-config" "--includedir"
  OUTPUT_VARIABLE llvm_include_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE)
include_directories("${llvm_include_dir}")

execute_process(
  COMMAND "llvm-config" "--ldflags"
  OUTPUT_VARIABLE llvm_ldflags
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_SHARED_LINKER_FLAGS ${llvm_ldflags})
if(APPLE)
  set(CMAKE_MODULE_LINKER_FLAGS "${llvm_ldflags} -undefined dynamic_lookup")
endif()

execute_process(
  COMMAND "llvm-config" "--cxxflags"
  OUTPUT_VARIABLE llvm_cxxflags
  OUTPUT_STRIP_TRAILING_WHITESPACE)
list(APPEND CMAKE_CXX_FLAGS "${llvm_cxxflags}")

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" SRCS)
add_library("mruby-clang-checker" MODULE "${SRCS}")

enable_testing()
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/test" TESTS)
foreach(i ${TESTS})
  get_filename_component(test_name "${i}" NAME)
  add_test(
    NAME "${test_name}"
    COMMAND
    clang -cc1
    -load "${CMAKE_BINARY_DIR}/libmruby-clang-checker.so" -add-plugin mruby-clang-checker
    "-I${MRUBY_ROOT}/include" -fsyntax-only -verify "${i}")
endforeach()
