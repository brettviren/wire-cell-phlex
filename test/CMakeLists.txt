# wire-cell-phlex/test/CMakeLists.txt

# Step 1: trivial build-and-link smoke test.
add_executable(test_trivial test_trivial.cpp)
target_link_libraries(test_trivial PRIVATE wire_cell_phlex
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME trivial COMMAND test_trivial)

# Step 2: wcphlex wrapper type validation.
add_executable(test_data_types test_data_types.cpp)
target_link_libraries(test_data_types PRIVATE wire_cell_phlex phlex::model)
add_test(NAME data_types COMMAND test_data_types)

# Step 3: boundary source/sink buffer classes.
# This test directly uses WireCell Iface/Aux types, so it needs the Spack GCC
# runtime libstdc++ linked explicitly (see WIRECELL_RUNTIME_LIBS in the top-
# level CMakeLists.txt) to resolve GLIBCXX versioned symbols at link time.
add_executable(test_boundary test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE wire_cell_phlex
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME boundary COMMAND test_boundary)

# Step 4: WctExecutor class.
# Runs a full WCT Pgrapher sub-graph (frame passthrough) for 12 events via
# FrameFilter.  Requires WireCellPgraph plugin (loaded at runtime via dlopen).
add_executable(test_executor test_executor.cpp)
target_link_libraries(test_executor PRIVATE wire_cell_phlex
  ${WIRECELL_RUNTIME_LIBS})
# Inject the path to cfg/ so the test can locate frame-passthrough.jsonnet.
target_compile_definitions(test_executor PRIVATE
  WCP_CFG_DIR="${CMAKE_SOURCE_DIR}/cfg"
)
add_test(NAME executor COMMAND test_executor)
