# SDK and Binding Design

Status: proposed

`open`, `runtime_info`, and `formats` are implemented for C++; see the
[architecture overview](../architecture/overview.md) for what that covers and
[sdk/README.md](../../sdk/README.md) for how to build it. Everything else on
this page -- `inspect` and every binding -- is intent, not implementation.

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

`formats()` does this against `schemas/formats.v1.json`, and it reports none of
the three candidate answers. The capabilities the composition resolved and the
formats OpenUSD dispatches differ exactly when a plugin fails to load, which is
when the answer matters most, so choosing either list would report a
composition that is broken as if it were fine or as if the format had never
been asked for, and reporting their intersection would hide the failure
entirely. Every extension is therefore reported with the state that says which
source claimed it -- `available`, `not_loaded`, `undeclared` -- and a caller
that only wants what it can open filters on `available` in one predicate. The
rule is a pure function of the two lists, so it lives in the OpenUSD-free lane
and is tested without a runtime; only obtaining the second list needs a loaded
OpenUSD.

That list must be what OpenUSD will actually dispatch, which is not what its
plugin metadata declares. The metadata index names an extension whether or not
the library behind it can load, so a report built from it would contradict
`open()` on the one runtime state the operation exists to name. Each candidate
is asked for its file format instead, which costs the load of every registered
format plugin -- the price of an introspection answer that agrees with the
operation it predicts.

`inspect()` remains undefined. It needs a stated result -- what it reports for
an asset that opens, and what it reports for one that does not -- before it has
a shape, and it should not be added until a test can prove that contract.

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
