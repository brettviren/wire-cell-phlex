#pragma once
// wire_cell_phlex/Executor.h
//
// WctExecutor: manages a WireCell::Main instance for per-event use within a
// PHLEX concurrency::serial transform node.
//
// Full implementation arrives in Step 4.  This header is a placeholder that
// establishes the namespace and will be expanded incrementally.

namespace wcphlex {

  /// Base class for WCT sub-graph executors.
  /// Subclasses (FrameFilter, DepoSetToFrame, …) are added in Step 4.
  class Executor {
  public:
    Executor() = default;
    virtual ~Executor() = default;

    Executor(Executor const&) = delete;
    Executor& operator=(Executor const&) = delete;
  };

} // namespace wcphlex
