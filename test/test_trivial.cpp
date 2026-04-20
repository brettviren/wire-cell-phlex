// test/test_trivial.cpp
//
// Step 1 smoke test: verifies the package builds and links against
// wire_cell_phlex without error.  No logic beyond return 0.

#include "wire_cell_phlex/Executor.h"

int main()
{
  // Instantiating the base class confirms the header and shared library
  // are reachable and that the vtable links correctly.
  wcphlex::Executor e;
  (void)e;
  return 0;
}
