# Design Specification

Status: proposed

## Objective

Evolve the existing reproducible composition into a geospatial runtime product
with a small, predictable consumer API while retaining OpenUSD as the native
data and programming model.

The intended user experience is eventually:

```cpp
auto result = usd_geospatial::open(uri);
```

```python
import usd_geospatial

stage = usd_geospatial.open(uri)
```

```js
import { open } from "usd-geospatial";

const stage = await open(uri);
```

These examples are design targets, not implemented APIs.

## System boundaries

```text
high-level convenience API
            |
   usd-geospatial SDK
            |
     OpenUSD native API
            |
 composed resolver and FileFormat plugins
```

The SDK provides stable entry points for common geospatial runtime operations;
it does not replace OpenUSD or conceal `pxr::*` from native users. Format
implementation remains in plugin repositories. Runtime composition and binary
artifact lifecycle remain delegated to OpenStrata.

## Composition contract

- Dependencies are expressed through capabilities wherever practical.
- Every release pins component and OCI identities in a deterministic lock.
- Runtime version and component versions remain distinct.
- Target identity is machine-readable and includes operating system,
  architecture, toolchain, OpenUSD version, and relevant host ABI.
- Adding a target creates a separately verifiable composition claim; support is
  never inferred from source portability alone.

## Consumer contract

- Public operation names are short and predictable: `open`, `inspect`,
  `formats`, and `runtime_info`.
- Failures expose stable codes and structured details instead of requiring
  message parsing.
- Introspection exposes installed formats, component versions, target identity,
  and exact runtime identity.
- C++, Python, and JavaScript bindings share a conceptual API even when language
  conventions differ.

The focused [SDK design](sdk.md), [distribution design](distribution.md), and
[testing design](testing.md) own the detailed contracts.

## Evolution constraints

Do not copy plugin implementations into this repository, make the runtime aware
of unnecessary format internals, bundle every native dependency into each
language package by default, or promise native/Wasm parity before the supported
subset is measured and documented.
