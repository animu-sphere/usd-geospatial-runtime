# Project Overview

`usd-geospatial-runtime` is the consumer-facing integration layer for a
reproducible OpenUSD geospatial runtime. It selects independently published
components, locks their exact artifacts, composes them with OpenStrata, verifies
the result, and publishes a runtime that can be reconstructed and inspected.

It is not a monorepo for geospatial format implementations. HTTP resolution,
point-cloud formats, raster formats, vector formats, and future domain-specific
features remain independently versioned plugins and artifacts.

## Composition model

```text
Applications and language bindings
                |
      stable consumer surface
                |
     usd-geospatial-runtime
                |
 OpenStrata runtime composition
                |
 OpenUSD + resolver + format plugins
```

Composition requirements depend on capabilities such as
`usd-fileformat:copc`, not repository names. Provider selection and immutable
artifact identities belong in the composition manifest and lock.

## Ownership boundaries

Plugin repositories own:

- format parsing and detection;
- OpenUSD authoring;
- format-specific diagnostics, fixtures, and tests; and
- standalone artifact publication.

This repository owns:

- compatible component selection and dependency locking;
- cross-plugin integration and runtime acceptance;
- composed runtime distribution and release evidence; and
- the future stable SDK and language-binding entry points.

OpenStrata owns generic capability resolution, composition, reconstruction,
artifact verification, SBOM, provenance, and OCI lifecycle features. Missing
generic composition features should be implemented there instead of duplicated
in this repository.

## Product principles

- **Composition over monolith.** Keep specialist implementations independent.
- **The runtime is a product.** A release includes immutable inputs, digests,
  target identity, SBOM, provenance, acceptance evidence, and a release record.
- **Small consumer surface.** Add stable convenience APIs without hiding or
  replacing OpenUSD's native API.
- **Inspectable behavior.** Runtime capabilities, versions, and failures should
  be machine-readable and deterministic.
- **Measured distribution decisions.** Select Python and JavaScript binary
  delivery models using size, install-time, startup, cache, offline, and CI
  measurements.

## Non-goals

The current direction does not include a custom geospatial scene graph, a full
OpenUSD wrapper, a renderer or viewer application, an independent package
registry, or copying plugin sources into this repository. It also does not
require all operating systems, bindings, or native plugins to ship at once.
