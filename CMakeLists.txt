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
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
