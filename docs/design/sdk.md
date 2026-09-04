# SDK and Binding Design

Status: proposed

No API on this page is implemented yet.

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

Failures should carry:

- a stable diagnostic code such as `UGEO-E014`;
- a category and human-readable message;
- the originating subsystem; and
- optional structured details such as a missing capability.

Message text is not an automation contract. C++ should use an explicit
`Result<T>`-style value, and language bindings should preserve the same code and
details through idiomatic errors or exceptions.

## Introspection

`formats()` and `runtime_info()` should report machine-readable values for the
active target, runtime identity, component versions, and available
capabilities. Equivalent input should produce stable field names and diagnostic
shapes so scripts and AI tools do not need to scrape display text.

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
