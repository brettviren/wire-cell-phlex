cmake_minimum_required(VERSION 3.25)
project(wire_cell_phlex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress spurious warnings present in GCC 14/15 headers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
      add_compile_options(-Wno-stringop-overflow)
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
      add_compile_options(-Wno-array-bounds)
    endif()
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# When using a Spack view, the installed shared libraries may have been built
# with a newer GCC runtime (e.g. GCC 15) than the system compiler provides.
# This causes versioned-symbol link errors (GLIBCXX_3.4.32, CXXABI_1.3.15)
# when any target directly links a Spack WireCell or Boost library.
#
# Fix: explicitly link the Spack libstdc++.so.6 (resolved from the symlink in
# the view's lib/ dir) for any target that needs it, and propagate it via the
# WIRECELL_RUNTIME_LIBS variable used in test/CMakeLists.txt.
set(WIRECELL_RUNTIME_LIBS "")
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
  if(EXISTS "${_prefix}/lib/libstdc++.so.6")
    link_directories(BEFORE "${_prefix}/lib")
    file(REAL_PATH "${_prefix}/lib/libstdc++.so.6" _stdc_real)
    get_filename_component(_stdc_dir "${_stdc_real}" DIRECTORY)
    # Use the full .so.6 path (no bare -lstdc++ symlink in Spack runtime dirs).
    set(WIRECELL_RUNTIME_LIBS "${_stdc_real}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS
      " -Wl,-rpath,${_stdc_dir}")
    break()
  endif()
endforeach()

find_package(TBB REQUIRED)
find_package(phlex REQUIRED)
find_package(WireCell REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

add_compile_options(-Wall -Wpedantic -Werror)

# ---------------------------------------------------------------------------
# wire_cell_phlex — shared library
#
# Contains the WCT/PHLEX integration code: data wrapper types, boundary
# source/sink buffers, and the WctExecutor class.  This library has no PHLEX
# module registration; downstream MODULE targets link it.
# ---------------------------------------------------------------------------
add_library(wire_cell_phlex SHARED
  wire_cell_phlex/Executor.cpp
  wire_cell_phlex/BoundaryNodes.cpp
  wire_cell_phlex/TrivialDepoFramer.cpp
)

target_include_directories(wire_cell_phlex
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(wire_cell_phlex
  PUBLIC
    WireCell::Apps
    WireCell::Iface
    WireCell::Aux
    WireCell::Util
    phlex::core
    Boost::json
)

# ---------------------------------------------------------------------------
# PHLEX MODULE targets (Step 5+)
#
# Each MODULE is a dlopen-only shared library loaded by the phlex binary.
# PHLEX prepends 'lib' and appends '.so' to the 'cpp' field in the workflow
# JSON, so add_library(wcp_frame_source MODULE ...) → libwcp_frame_source.so,
# matching  cpp: 'wcp_frame_source'  in the workflow.
#
# These modules must be compiled with GCC 15 (required by phlex headers).
# ---------------------------------------------------------------------------
add_library(wcp_frame_source MODULE modules/frame_source.cpp)
target_link_libraries(wcp_frame_source PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_frame_source PRIVATE -UNDEBUG)

add_library(wcp_frame_filter MODULE modules/frame_filter.cpp)
target_link_libraries(wcp_frame_filter PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_frame_filter PRIVATE -UNDEBUG)

# Step 5 (continued): single-frame observer for end-cap validation.
add_library(wcp_frame_observer MODULE modules/frame_observer.cpp)
target_link_libraries(wcp_frame_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_frame_observer PRIVATE -UNDEBUG)

# Step 7: two-frame observer for multi-instance validation.
add_library(wcp_two_frame_observer MODULE modules/two_frame_observer.cpp)
target_link_libraries(wcp_two_frame_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_two_frame_observer PRIVATE -UNDEBUG)

# Step 6: IDepoSet → IFrame executor module.
add_library(wcp_deposet_source MODULE modules/deposet_source.cpp)
target_link_libraries(wcp_deposet_source PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_deposet_source PRIVATE -UNDEBUG)

add_library(wcp_deposet_to_frame MODULE modules/deposet_to_frame.cpp)
target_link_libraries(wcp_deposet_to_frame PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcp_deposet_to_frame PRIVATE -UNDEBUG)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)

# Step 5: PHLEX integration test — frame source → WCT passthrough → done.
find_program(PHLEX_EXECUTABLE phlex
  HINTS "${CMAKE_PREFIX_PATH}" ENV PATH
  PATH_SUFFIXES bin
  REQUIRED
)

# Derive the directory where phlex framework plugins live (generate_layers etc.).
get_filename_component(_phlex_bin_dir "${PHLEX_EXECUTABLE}" DIRECTORY)
get_filename_component(_phlex_lib_dir "${_phlex_bin_dir}/../lib" ABSOLUTE)

set(_phlex_env
  "PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}:${_phlex_lib_dir}"
  "WIRECELL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cfg"
)

add_test(NAME phlex_frame_filter
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/frame-filter-workflow.jsonnet"
)
set_tests_properties(phlex_frame_filter PROPERTIES ENVIRONMENT "${_phlex_env}")

add_test(NAME phlex_deposet_to_frame
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-to-frame-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_to_frame PROPERTIES ENVIRONMENT "${_phlex_env}")

add_test(NAME phlex_multi_instance
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/multi-instance-workflow.jsonnet"
)
set_tests_properties(phlex_multi_instance PROPERTIES ENVIRONMENT "${_phlex_env}")
