# Generated config-schema factories

These `*.libjsonnet` files are **generated** — do not edit by hand.

Each is a Jsonnet constructor function for one Executor node's configuration
schema (see `wire_cell_phlex/Config.hpp`), with the field docstrings carried
through as `@param` comments. They are emitted from the C++ schema structs, so
they always match what the nodes actually parse.

A workflow config can import a factory to build a documented, defaulted config
object, e.g.:

```jsonnet
local FrameFilterConfig = import "schema/framefilterconfig.libjsonnet";
FrameFilterConfig(executor = ExecutorConfig(wct_config = "my-graph.jsonnet"))
```

## Regenerating

Build the plugins, then run the boost-config discovery CLI against them (with
the plugin dependencies on `LD_LIBRARY_PATH`):

```bash
boost-config-schemas -o cfg/schema builds/envs/<env>/wire-cell-phlex
```

The plugins advertise their schemas via `BOOST_CONFIG_EXPORT` in each module
`.cpp`; the CLI (`devel/boost-config/tools/boost-config-schemas`) discovers and
emits them.
