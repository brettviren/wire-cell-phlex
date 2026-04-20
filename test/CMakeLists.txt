# wire-cell-phlex/test/CMakeLists.txt
#
# Step 1: trivial build-and-link smoke test.
# Additional test targets are added in subsequent steps.

add_executable(test_trivial test_trivial.cpp)
target_link_libraries(test_trivial PRIVATE wire_cell_phlex)

add_test(NAME trivial COMMAND test_trivial)
