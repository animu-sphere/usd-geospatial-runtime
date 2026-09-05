# SDK and Binding Design

Status: proposed

`open` and `runtime_info` are implemented for C++; see the
[architecture overview](../architecture/overview.md) for what that covers and
[sdk/README.md](../../sdk/README.md) for how to build it. Everything else on
this page -- `inspect`, `formats`, and every binding -- is intent, not
implementation.

## Initial C++ surface

The first SDK should provide only the operations needed to establish a stable
consumer boundary:

```text
open
inspect
formats
runtime_info
```

OpenUSD remains available below this convenience layer. The SDK should return
an OpenUSD-compatible stage or a small result object that provides access to
one, rather than introducing a second scene graph.

## Results and diagnostics

Failures carry:

- a stable diagnostic code such as `UGEO-E014`;
- a category and human-readable message;
- the originating subsystem; and
- optional structured details such as a missing capability.

Message text is not an automation contract. C++ uses an explicit `Result<T>`
value, and language bindings should preserve the same code and details through
idiomatic errors or exceptions. The published codes are listed in the
[diagnostics reference](../reference/diagnostics.md).

## Introspection

`formats()` and `runtime_info()` should report machine-readable values for the
active target, runtime identity, component versions, and available
capabilities. Equivalent input should produce stable field names and diagnostic
shapes so scripts and AI tools do not need to scrape display text.

`runtime_info()` does this today, against `schemas/runtime-info.v1.json`. It
reads the materialized prefix rather than asking OpenUSD, so it can describe a
runtime -- or report that there is none -- before any OpenUSD library loads.
`formats()` does not exist yet: it must first be decided whether it reports the
capabilities the composition resolved, the extensions OpenUSD actually
registered, or the intersection, because those differ exactly when a plugin
fails to load, which is when the answer matters most.

## Python

The intended package and import names are `usd-geospatial` and
`usd_geospatial`. The initial supported binding may target CPython 3.13. An
`abi3` shim is worth evaluating, but it is not sufficient if exposed OpenUSD
Python objects still require a version-specific CPython ABI.

The Python package should remain a thin binding and runtime-discovery layer
unless distribution measurements justify embedding the complete runtime.

## JavaScript

The intended package is ESM-first and follows the same conceptual operations as
C++. Native Node support should follow stabilization of the C++ contract.
Platform packages and OCI-backed acquisition are both candidates until measured.

## Wasm

Wasm is a portable, read-oriented subset rather than a promise to reproduce the
entire native plugin set. The first experiment should cover format detection,
metadata inspection, bounds, and small reads through an ES module wrapper that
does not depend on one bundler.
