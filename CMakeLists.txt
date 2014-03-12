cmake_minimum_required(VERSION 2.8)

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
if(LINUX)
  # TODO
endif()

execute_process(
  COMMAND "llvm-config" "--cxxflags"
  OUTPUT_VARIABLE llvm_cxxflags
  OUTPUT_STRIP_TRAILING_WHITESPACE)
list(APPEND CMAKE_CXX_FLAGS "${llvm_cxxflags}")

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" SRCS)

add_library("mruby-clang-checker" MODULE "${SRCS}")
