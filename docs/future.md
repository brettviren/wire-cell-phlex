# Future development ideas

## Reducing redundancy between code and config.

In the current design the user must configure PHLEX to use a WCPh `Executor` (eg
a `FrameFilter`) and a graph that is bookended by a `BoundarySource` and
`BoundarySink` of the right type (eg `FrameBoundarySource` and
`FrameBoundarySink`).  A mismatch will be some error.

A concrete `Executor` "knows" the type of the /boundary converters/, assuming
there is always only one for a given source/sink data type.  This can almost
certainly be guaranteed as a boundary converter is basically a deque that
marshals data of the given type in a trivial way.  If non-trivial boundary
conversion is needed then it can live either as a pure PHLEX or pure WCT node.

However, making the concrete `Executor` in charge of providing the boundary
converters means that user configuration produces an incomplete graph and
something new is needed in WCT to allow a concrete `Executor` to "amend" that
graph with the boundary converters prior to finalizing the config.  In
principle, this is not a big problem as the concrete executor is creating and
holding the `WireCell::Main` object.

## The =wire_schema_source= 

It currently makes a new =WireSchema= object per call.  It should cache its
payload and return that each time.

# Bugs

- Boundary converters need to use deque not queue

