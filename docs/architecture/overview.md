# Architecture Overview

## Current state

The repository currently publishes one immutable OpenStrata composition for
`windows-x86_64-msvc143-py313`. It combines OpenUSD 26.08, an HTTP resolver and
its asset I/O/cache dependencies, point-cloud FileFormat plugins for LAS, LAZ,
COPC, and PLY, and a raster FileFormat plugin for GeoTIFF.

The repository does not yet implement a `usd_geospatial` C++ SDK, Python
package, Node package, Wasm module, vector provider, or Linux/macOS composition.

## Repository surfaces

| Path | Responsibility |
| --- | --- |
| `runtime-composition.windows.toml` | Declares the target, required capabilities, immutable candidate artifacts, and explicit providers. |
| `runtime.windows.lock.json` | Pins resolution and artifact identities for reproducible composition. |
| `tools/accept.py` | Runs installed runtime probes and writes detailed acceptance output outside the immutable composition. |
| `fixtures/three-points.copc` | Supplies a small deterministic COPC integration input. |
| `tests/native-consumer` | Verifies that a separate CMake consumer can find and link the composed `usdAssetIo` package. |
| `evidence/v0.1.0-windows.json` | Preserves the concise accepted-release result and immutable runtime identities. |
| `docs/releases` | Records versioned scope, inputs, distribution identities, results, and limitations. |

## Composition flow

```text
capability requirements + candidate artifacts + provider mapping
                            |
                 OpenStrata locked compose
                            |
             immutable materialized runtime
                            |
        installed SDK and format/resolver probes
                            |
              acceptance output and evidence
                            |
       composed artifact + SBOM + provenance + OCI
```

The manifest requests capabilities rather than depending directly on plugin
repository names. Its provider table selects the artifact that satisfies each
capability. The committed lock preserves the resolved result, and `--locked`
composition prevents unreviewed dependency movement.

## Acceptance boundary

The acceptance runner executes probes installed by the composed artifacts. It
validates the SDK surface supplied by the composition, HTTP cold and warm cache
behavior, LAS/LAZ/PLY/COPC reads, ten local and HTTP COPC validator scenarios,
and GeoTIFF metadata authoring. Evidence must be written outside the immutable
runtime prefix.

Release acceptance additionally pulls the composed artifact into an empty OST
registry, verifies its identities and metadata, reconstructs it, and reruns the
runtime checks. Passing inside a producer build directory alone is not a
release claim.

## Distribution boundary

Source and composition declarations live in Git. Runtime binaries are
content-addressed OpenStrata artifacts distributed through OCI. The v0.1.0
artifact includes an SPDX SBOM and in-toto provenance. Python 3.13 is a host
prerequisite and is not bundled in the composition.

Exact current support claims are maintained in the
[support matrix](../reference/support-matrix.md), and immutable release values
are maintained in the [release record](../releases/v0.1.0.md).
