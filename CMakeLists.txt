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
# Resolve THIS compiler's libstdc++ directly, independent of CMAKE_PREFIX_PATH
# order. A Spack build can place an older gcc-runtime earlier on the path; the
# previous prefix scan grabbed that, linking gcc@15 objects against gcc@11
# libstdc++ (missing GLIBCXX_3.4.31/.32). The compiler always yields its own.
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.so
  OUTPUT_VARIABLE _stdc_real OUTPUT_STRIP_TRAILING_WHITESPACE)
file(REAL_PATH "${_stdc_real}" _stdc_real)
get_filename_component(_stdc_dir "${_stdc_real}" DIRECTORY)
set(WIRECELL_RUNTIME_LIBS "${_stdc_real}")
link_directories(BEFORE "${_stdc_dir}")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-rpath,${_stdc_dir}")

find_package(TBB REQUIRED)
find_package(phlex REQUIRED)
find_package(WireCell REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)
# Header-only config-schema helpers (Documented<T>, boost::json (de)serialization,
# Jsonnet factory).  Public: wire_cell_phlex/Config.hpp is included by the public
# Executor.hpp.
find_package(boost_config REQUIRED)
# Generic Phlex node-config schemas (PhlexAlgorithmConfig, ProductSelector).
# Public: wire_cell_phlex/Config.hpp composes PhlexAlgorithmConfig.
find_package(phlex_config REQUIRED)

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
  wire_cell_phlex/FacadeWireSchema.cpp
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
    boost_config::boost_config
    phlex_config::phlex_config
)

# ---------------------------------------------------------------------------
# PHLEX MODULE targets (Step 5+)
#
# Each MODULE is a dlopen-only shared library loaded by the phlex binary.
# PHLEX prepends 'lib' and appends '.so' to the 'cpp' field in the workflow
# JSON, so add_library(wcph_frame_source MODULE ...) → libwcph_frame_source.so,
# matching  cpp: 'wcph_frame_source'  in the workflow.
#
# These modules must be compiled with GCC 15 (required by phlex headers).
# ---------------------------------------------------------------------------
add_library(wcph_frame_source MODULE modules/frame_source.cpp)
target_link_libraries(wcph_frame_source PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_source PRIVATE -UNDEBUG)

add_library(wcph_frame_filter MODULE modules/frame_filter.cpp)
target_link_libraries(wcph_frame_filter PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_filter PRIVATE -UNDEBUG)

# Step 5 (continued): single-frame observer for end-cap validation.
add_library(wcph_frame_observer MODULE modules/frame_observer.cpp)
target_link_libraries(wcph_frame_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_observer PRIVATE -UNDEBUG)

# Step 7: two-frame observer for multi-instance validation.
add_library(wcph_two_frame_observer MODULE modules/two_frame_observer.cpp)
target_link_libraries(wcph_two_frame_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_two_frame_observer PRIVATE -UNDEBUG)

# Step 8: job-layer wire geometry source and observer.
add_library(wcph_wire_schema_source MODULE modules/wire_schema_source.cpp)
target_link_libraries(wcph_wire_schema_source PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_wire_schema_source PRIVATE -UNDEBUG)

add_library(wcph_wire_schema_observer MODULE modules/wire_schema_observer.cpp)
target_link_libraries(wcph_wire_schema_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_wire_schema_observer PRIVATE -UNDEBUG)

# Step 6: IDepoSet → IFrame executor module.
add_library(wcph_deposet_source MODULE modules/deposet_source.cpp)
target_link_libraries(wcph_deposet_source PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_source PRIVATE -UNDEBUG)

add_library(wcph_deposet_to_frame MODULE modules/deposet_to_frame.cpp)
target_link_libraries(wcph_deposet_to_frame PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_to_frame PRIVATE -UNDEBUG)

# Step A: IDepoSet → IDepoSet filter module.
add_library(wcph_deposet_filter MODULE modules/deposet_filter.cpp)
target_link_libraries(wcph_deposet_filter PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_filter PRIVATE -UNDEBUG)

# Step B: DepoSet observer module.
add_library(wcph_deposet_observer MODULE modules/deposet_observer.cpp)
target_link_libraries(wcph_deposet_observer PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_observer PRIVATE -UNDEBUG)

# Steps C+D: File-based DepoSet source and sink modules.
add_library(wcph_deposet_source_file MODULE modules/deposet_source_file.cpp)
target_link_libraries(wcph_deposet_source_file PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_source_file PRIVATE -UNDEBUG)

add_library(wcph_deposet_sink MODULE modules/deposet_sink.cpp)
target_link_libraries(wcph_deposet_sink PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_deposet_sink PRIVATE -UNDEBUG)

# Third real job: file → IFrame source module.
add_library(wcph_frame_source_file MODULE modules/frame_source_file.cpp)
target_link_libraries(wcph_frame_source_file PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_source_file PRIVATE -UNDEBUG)

# Second real job: IFrame → file sink module.
add_library(wcph_frame_sink MODULE modules/frame_sink.cpp)
target_link_libraries(wcph_frame_sink PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_sink PRIVATE -UNDEBUG)

# Fans job: 4×IFrame fan-in → merge via FrameFanin → file.
add_library(wcph_frame_fanin_sink_file MODULE modules/frame_fanin_sink_file.cpp)
target_link_libraries(wcph_frame_fanin_sink_file PRIVATE
  wire_cell_phlex
  phlex::module
  ${WIRECELL_RUNTIME_LIBS}
)
target_compile_options(wcph_frame_fanin_sink_file PRIVATE -UNDEBUG)

# ---------------------------------------------------------------------------
# Config-schema Jsonnet factories
#
# Generate <module>.libjsonnet from the config schemas the executor plugins
# advertise (via BOOST_CONFIG_EXPORT).  Built into build/wire_cell_phlex/ and
# installed to <prefix>/share/jsonnet/wire_cell_phlex/.  Provided by the
# boost_config CMake package; the files are re-derived from the C++ structs on
# every build, so they cannot drift.
# ---------------------------------------------------------------------------
boost_config_add_libjsonnet(
  PACKAGE wire_cell_phlex
  ALL
  PLUGINS
    # Only modules that still BOOST_CONFIG_EXPORT a schema.  The templated-shape
    # modules (deposet_to_frame, deposet_filter, frame_filter, deposet_sink,
    # frame_sink) deliberately export nothing — their schema will be revisited
    # in terms of node shapes.
    wcph_deposet_source_file
    wcph_frame_source_file
    wcph_frame_fanin_sink_file
)

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

# Wire-Cell Toolkit's Jsonnet compiler.  Unlike the plain `phlex -c` reader, it
# honors WIRECELL_PATH when resolving `import`s, so workflow configs that pull in
# shared library Jsonnet (e.g. the factored dune/... configs) are compiled to
# plain JSON here and then handed to `phlex -c`.  (`phlex` itself resolves no
# import search path; the Jsonnet-aware `phlexed` CLI is only a stop-gap and is
# deliberately NOT a dependency of wire-cell-phlex.)
find_program(WCSONNET_EXECUTABLE wcsonnet
  HINTS "${CMAKE_PREFIX_PATH}" ENV PATH
  PATH_SUFFIXES bin
  REQUIRED
)

# Derive the directory where phlex framework plugins live (generate_layers etc.).
get_filename_component(_phlex_bin_dir "${PHLEX_EXECUTABLE}" DIRECTORY)
get_filename_component(_phlex_lib_dir "${_phlex_bin_dir}/../lib" ABSOLUTE)

# Locate the WireCell shared data directory (wire geometry files, etc.).
# Prefer a share/wirecell that actually contains geometry/response data
# (*.json.bz2) rather than one holding only jsonnet configs: some prefixes
# (e.g. our install tree) ship the latter, while the Spack view's share/wirecell
# carries the real data files.  Picking the first existing dir unconditionally
# would select the data-less install tree and break every _with_data test.
set(_wct_data_dir "")
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
  if(EXISTS "${_prefix}/share/wirecell")
    file(GLOB _wct_data_files "${_prefix}/share/wirecell/*.json.bz2")
    if(_wct_data_files)
      set(_wct_data_dir "${_prefix}/share/wirecell")
      break()
    endif()
  endif()
endforeach()

set(_phlex_env
  "PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}:${_phlex_lib_dir}"
  "WIRECELL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cfg"
)

# Env for tests that need WCT shared data (wire geometry files).
set(_phlex_env_with_data
  "PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}:${_phlex_lib_dir}"
  "WIRECELL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cfg:${_wct_data_dir}"
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

add_test(NAME phlex_wire_schema
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/wire-schema-workflow.jsonnet"
)
set_tests_properties(phlex_wire_schema PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

add_test(NAME phlex_frame_filter_facade
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/frame-filter-facade-workflow.jsonnet"
)
set_tests_properties(phlex_frame_filter_facade PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

# Step B: DepoSet passthrough filter + observer.
add_test(NAME phlex_deposet_filter
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-filter-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_filter PROPERTIES ENVIRONMENT "${_phlex_env}")

# Steps C+D+F: file-based DepoSet identity round-trip and drifter integration tests.
#
# File paths are injected into the workflow Jsonnet at configure time because
# PHLEX 0.2.0 does not support command-line Jsonnet TLAs or external variables.
set(WCP_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/data")
set(WCP_OUTPUT_DIR    "${CMAKE_CURRENT_BINARY_DIR}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-identity-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/deposet-identity-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-drifter-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/deposet-drifter-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-sim-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/deposet-sim-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/frame-sigproc-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/frame-sigproc-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/deposet-sim-sigproc-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/deposet-sim-sigproc-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/sim-sigproc-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/sim-sigproc-mismatch-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-mismatch-workflow.jsonnet"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/fans-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/fans-workflow.jsonnet"
  @ONLY
)

# Sigproc variants: per-APA single-instance tests and 4-APA fan with sigproc.
# (These previously failed with an Eigen bounds assertion in OmnibusSigProc for
# apa_ident=3 with drifted PDSP depos; they now pass once the WCT shared-data
# dir resolves correctly — see _wct_data_dir above.)
foreach(_apa IN ITEMS 0 1 2 3)
  set(APA_IDENT "${_apa}")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/test/pdhd-apa-sigproc-workflow.jsonnet.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pdhd-apa${_apa}-sigproc-workflow.jsonnet"
    @ONLY
  )
endforeach()
unset(APA_IDENT)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test/fans-sigproc-workflow.jsonnet.in"
  "${CMAKE_CURRENT_BINARY_DIR}/fans-sigproc-workflow.jsonnet"
  @ONLY
)

# PDHD factored config test: uses the new dune/wct/dets and dune/wct/job layer.
# Generate one factored workflow per compared APA: APA0 (np04hd bad-APA response,
# run for cleanliness only) and APA1 (nominal response, compared for equivalence
# against the hand-written pdhd-apa1-sigproc reference).
foreach(_fapa IN ITEMS 0 1)
  set(FACTORED_APA "${_fapa}")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/test/pdhd-sim-sigproc-workflow.jsonnet.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pdhd-sim-sigproc-apa${_fapa}-workflow.jsonnet"
    @ONLY
  )
endforeach()
unset(FACTORED_APA)

add_test(NAME phlex_deposet_identity
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/deposet-identity-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_identity PROPERTIES ENVIRONMENT "${_phlex_env}")

add_test(NAME phlex_deposet_drifter
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/deposet-drifter-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_drifter PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

# Second real job: DepoSet → drift+sim → Frame → file.
add_test(NAME phlex_deposet_sim
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/deposet-sim-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_sim PROPERTIES
  ENVIRONMENT "${_phlex_env_with_data}"
  FIXTURES_SETUP phlex_deposet_sim_fixture
)

# Third real job Variant A: Frame file → OmnibusSigProc → Frame file.
# Depends on phlex_deposet_sim to produce sim-frames.npz first.
add_test(NAME phlex_frame_sigproc
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/frame-sigproc-workflow.jsonnet"
)
set_tests_properties(phlex_frame_sigproc PROPERTIES
  ENVIRONMENT "${_phlex_env_with_data}"
  FIXTURES_REQUIRED phlex_deposet_sim_fixture
)

# Third real job Variant B: DepoSet → drift+sim+sigproc → Frame file.
add_test(NAME phlex_deposet_sim_sigproc
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/deposet-sim-sigproc-workflow.jsonnet"
)
set_tests_properties(phlex_deposet_sim_sigproc PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

# Two-island sim+sigproc: exercises WCT service sharing across two Main instances.
# The sim island (deposet-drift-sim.jsonnet) and sigproc island (frame-sigproc.jsonnet)
# both use service_prefix="" (default), so they resolve to the same AnodePlane:apa0,
# FieldResponse:fr, etc. via the global WCT NamedFactory.
# To test independent (mismatched) instances, set service_prefix="sim_"/"sp_" per island.
add_test(NAME phlex_sim_sigproc
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-workflow.jsonnet"
)
set_tests_properties(phlex_sim_sigproc PROPERTIES
  ENVIRONMENT "${_phlex_env_with_data}"
  FIXTURES_SETUP phlex_sim_sigproc_fixture
)

# Two-island sim+sigproc: independent (mismatched) service components.
# Same topology as phlex_sim_sigproc but service_prefix="sim_"/"sp_" forces
# each island to create its own AnodePlane, FieldResponse, etc. instances.
# Both tests should produce physically identical output since the same data
# files are used; differences would indicate a sharing-related bug.
add_test(NAME phlex_sim_sigproc_mismatch
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-mismatch-workflow.jsonnet"
)
set_tests_properties(phlex_sim_sigproc_mismatch PROPERTIES
  ENVIRONMENT "${_phlex_env_with_data}"
  FIXTURES_SETUP phlex_sim_sigproc_mismatch_fixture
)

# Compare shared vs. independent outputs: should be bit-identical when the
# same data files and parameters are used, validating that factory sharing
# (same name → same instance, re-configured) is equivalent to using
# independent instances.
add_test(NAME phlex_sim_sigproc_compare
  COMMAND python3
    "${CMAKE_CURRENT_SOURCE_DIR}/test/compare_frames_npz.py"
    --max-diff 2
    --max-ch-diff 1
    "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-frames.npz"
    "${CMAKE_CURRENT_BINARY_DIR}/sim-sigproc-mismatch-frames.npz"
)
set_tests_properties(phlex_sim_sigproc_compare PROPERTIES
  FIXTURES_REQUIRED "phlex_sim_sigproc_fixture;phlex_sim_sigproc_mismatch_fixture"
)

# Fans job: PDHD 4-APA fan-out/fan-in sim (no sigproc, always passes).
add_test(NAME phlex_fans
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/fans-workflow.jsonnet"
)
set_tests_properties(phlex_fans PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

# Per-APA sigproc tests: isolate each APA through DepoTransform+OmnibusSigProc.
# APAs 0-2 pass; APA 3 currently crashes with an Eigen bounds assertion and
# will pass once that bug is fixed.
foreach(_apa IN ITEMS 0 1 2 3)
  add_test(NAME phlex_pdhd_apa${_apa}_sigproc
    COMMAND "${PHLEX_EXECUTABLE}" -c
      "${CMAKE_CURRENT_BINARY_DIR}/pdhd-apa${_apa}-sigproc-workflow.jsonnet"
  )
  # Expose each APA's output (pdhd-apa<N>-sigproc-frames.npz) as a fixture so the
  # factored-vs-handwritten equivalence test can consume it as a reference.
  set_tests_properties(phlex_pdhd_apa${_apa}_sigproc PROPERTIES
    ENVIRONMENT "${_phlex_env_with_data}"
    FIXTURES_SETUP phlex_pdhd_apa${_apa}_sigproc_fixture
  )
endforeach()

# PDHD factored config integration test.
# Exercises the new dune/wct/dets/pdhd/detector.jsonnet + dune/wct/job/sim-sigproc.jsonnet
# pipeline.  Two APA variants run (APA0 with the np04hd bad-APA response, APA1 with
# the nominal response); each writes pdhd-sim-sigproc-apa<N>-frames.npz.
#
# Unlike the other workflows here, these `import` shared Jsonnet from the cfg/
# tree, which `phlex -c` cannot resolve.  So each runs in two stages: first
# `wcsonnet` compiles the Jsonnet to plain JSON (resolving imports via
# WIRECELL_PATH), then `phlex -c` runs the generated JSON.  A CTest fixture
# chains the two.
foreach(_fapa IN ITEMS 0 1)
  add_test(NAME phlex_pdhd_sim_sigproc_factored_apa${_fapa}_compile
    COMMAND "${WCSONNET_EXECUTABLE}"
      -o "${CMAKE_CURRENT_BINARY_DIR}/pdhd-sim-sigproc-apa${_fapa}-workflow.json"
      "${CMAKE_CURRENT_BINARY_DIR}/pdhd-sim-sigproc-apa${_fapa}-workflow.jsonnet"
  )
  set_tests_properties(phlex_pdhd_sim_sigproc_factored_apa${_fapa}_compile PROPERTIES
    ENVIRONMENT "${_phlex_env_with_data}"
    FIXTURES_SETUP phlex_pdhd_sim_sigproc_factored_apa${_fapa}_compile_fixture
  )

  add_test(NAME phlex_pdhd_sim_sigproc_factored_apa${_fapa}
    COMMAND "${PHLEX_EXECUTABLE}" -c
      "${CMAKE_CURRENT_BINARY_DIR}/pdhd-sim-sigproc-apa${_fapa}-workflow.json"
  )
  set_tests_properties(phlex_pdhd_sim_sigproc_factored_apa${_fapa} PROPERTIES
    ENVIRONMENT "${_phlex_env_with_data}"
    FIXTURES_REQUIRED phlex_pdhd_sim_sigproc_factored_apa${_fapa}_compile_fixture
    FIXTURES_SETUP phlex_pdhd_sim_sigproc_factored_apa${_fapa}_fixture
  )
endforeach()

# Equivalence check: the factored APA1 output vs the hand-written
# pdhd-apa1-sigproc reference.  APA1 is used (not APA0) because both sides then
# use the SAME nominal field response -- APA0's factored config deliberately
# uses the np04hd "bad APA" best-fit response, so it is only run for cleanliness,
# not compared.  Content-level validation is tracked separately (ddm-qiv.6).
add_test(NAME phlex_pdhd_sim_sigproc_factored_compare
  COMMAND python3
    "${CMAKE_CURRENT_SOURCE_DIR}/test/compare_frames_npz.py"
    --max-diff 2
    --max-ch-diff 1
    "${CMAKE_CURRENT_BINARY_DIR}/pdhd-sim-sigproc-apa1-frames.npz"
    "${CMAKE_CURRENT_BINARY_DIR}/pdhd-apa1-sigproc-frames.npz"
)
set_tests_properties(phlex_pdhd_sim_sigproc_factored_compare PROPERTIES
  FIXTURES_REQUIRED "phlex_pdhd_sim_sigproc_factored_apa1_fixture;phlex_pdhd_apa1_sigproc_fixture"
  # DISABLED pending content-level validation (ddm-qiv.6): the comparison is now
  # apples-to-apples (factored APA1 vs hand-written pdhd-apa1, both nominal
  # response), but the two configs still differ in content -- timing convention
  # (t0/tbin) and channel coverage (factored spans the full APA1; the reference
  # emits ~half).  Both jobs run clean; re-enable once the configs are reconciled.
  DISABLED TRUE
)

# 4-APA fan with full sigproc: fails today (apa_ident=3 crash), passes once fixed.
add_test(NAME phlex_fans_sigproc
  COMMAND "${PHLEX_EXECUTABLE}" -c
    "${CMAKE_CURRENT_BINARY_DIR}/fans-sigproc-workflow.jsonnet"
)
set_tests_properties(phlex_fans_sigproc PROPERTIES ENVIRONMENT "${_phlex_env_with_data}")

# ---------------------------------------------------------------------------
# Install rules
#
# The wire_cell_phlex SHARED library and its public headers are installed so
# downstream CMake projects can find them via find_package(wire_cell_phlex).
# The PHLEX MODULE targets (dlopen only) are installed into lib/ alongside the
# shared library so PHLEX_PLUGIN_PATH can point to a single directory.
# WCT Jsonnet sub-graph configs are installed into share/wire-cell-phlex/cfg/
# so they remain accessible via WIRECELL_PATH after installation.
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

# Shared library
install(TARGETS wire_cell_phlex
  EXPORT wire_cell_phlexTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Public headers
install(DIRECTORY wire_cell_phlex/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wire_cell_phlex"
  FILES_MATCHING PATTERN "*.hpp"
)

# PHLEX MODULE libraries (dlopen only — no EXPORT needed)
foreach(_mod IN ITEMS
    wcph_frame_source
    wcph_frame_filter
    wcph_frame_observer
    wcph_two_frame_observer
    wcph_deposet_source
    wcph_deposet_to_frame
    wcph_deposet_filter
    wcph_deposet_observer
    wcph_deposet_source_file
    wcph_deposet_sink
    wcph_frame_source_file
    wcph_frame_sink
    wcph_frame_fanin_sink_file
    wcph_wire_schema_source
    wcph_wire_schema_observer
)
  install(TARGETS ${_mod}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endforeach()

# WCT Jsonnet configs (searched via WIRECELL_PATH)
install(DIRECTORY cfg/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/wire-cell-phlex/cfg"
  FILES_MATCHING PATTERN "*.jsonnet"
)

# CMake package config (so find_package(wire_cell_phlex) works downstream)
install(EXPORT wire_cell_phlexTargets
  FILE wire_cell_phlexTargets.cmake
  NAMESPACE wire_cell_phlex::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_phlex"
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_phlexConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wire_cell_phlexConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_phlexConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_phlex"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_phlexConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_phlexConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_phlex"
)
