cmake_minimum_required(VERSION 2.8)

if(DEFINED ENV{LLVM_CONFIG})
  set(LLVM_CONFIG "$ENV{LLVM_CONFIG}" CACHE FILEPATH "llvm-config used in build")
else()
  set(LLVM_CONFIG "llvm-config")
endif()

if(EXISTS "$ENV{MRUBY_ROOT}")
  if(IS_ABSOLUTE "$ENV{MRUBY_ROOT}")
    set(MRUBY_ROOT "$ENV{MRUBY_ROOT}")
  else()
    get_filename_component(MRUBY_ROOT "$ENV{MRUBY_ROOT}" ABSOLUTE)
  endif()
  set(MRUBY_ROOT "${MRUBY_ROOT}" CACHE PATH "mruby root path used in test")
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

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" SRCS)
set(library_name "mruby-clang-checker")
add_library("${library_name}" MODULE "${SRCS}")

execute_process(
  COMMAND "${LLVM_CONFIG}" "--bindir"
  OUTPUT_VARIABLE llvm_bin_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE)
get_property(plugin_file TARGET "${library_name}" PROPERTY LOCATION)

enable_testing()
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/test" TESTS)
foreach(i ${TESTS})
  get_filename_component(test_name "${i}" NAME)
  add_test(
    NAME "${test_name}"
    COMMAND
    "${llvm_bin_dir}/clang" -cc1
    -load "${plugin_file}" -add-plugin "${library_name}"
    "-I${MRUBY_ROOT}/include" -fsyntax-only -verify "${i}")
endforeach()
