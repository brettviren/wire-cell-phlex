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

# (The former test_executor, which exercised the original FrameFilter /
# DepoSetFilter named executors directly, has been retired along with those
# executors; the templated executors are covered by test_function_executor,
# test_shape_executors, and the integration workflows.)

# Idea-2 spike (ddm-7dk.3): templated FunctionExecutor<In,Out> + generic
# mid-level-interface boundary nodes.
add_executable(test_function_executor test_function_executor.cpp)
target_link_libraries(test_function_executor PRIVATE wire_cell_phlex
  ${WIRECELL_RUNTIME_LIBS})
target_compile_definitions(test_function_executor PRIVATE
  WCP_CFG_DIR="${CMAKE_SOURCE_DIR}/cfg"
)
add_test(NAME function_executor COMMAND test_function_executor)

# Shape-executor family (Source/Sink/Function/Fanin/Fanout/Join/Split) — a
# compile-time proof that every shape instantiates.
add_executable(test_shape_executors test_shape_executors.cpp)
target_link_libraries(test_shape_executors PRIVATE wire_cell_phlex
  ${WIRECELL_RUNTIME_LIBS})
add_test(NAME shape_executors COMMAND test_shape_executors)
