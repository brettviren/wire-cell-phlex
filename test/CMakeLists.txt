# wire-cell-phlex/test/CMakeLists.txt

# Step 1: trivial build-and-link smoke test.
add_executable(test_trivial test_trivial.cpp)
target_link_libraries(test_trivial PRIVATE wire_cell_phlex)
add_test(NAME trivial COMMAND test_trivial)

# Step 2: wcphlex wrapper type validation.
add_executable(test_data_types test_data_types.cpp)
target_link_libraries(test_data_types PRIVATE wire_cell_phlex phlex::model)
add_test(NAME data_types COMMAND test_data_types)
