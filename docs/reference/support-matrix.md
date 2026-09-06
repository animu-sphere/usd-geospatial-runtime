# Support Matrix

## Composition targets

| Target | Status | OpenUSD | Host Python | Evidence |
| --- | --- | --- | --- | --- |
| `windows-x86_64-msvc143-py313` | Released in v0.1.0 | 26.08 | 3.13, not bundled | [v0.1.0 record](../releases/v0.1.0.md) |
| Linux | Not supported | - | - | None |
| macOS | Not supported | - | - | None |
| Wasm | Not supported | - | - | None |

Source portability or upstream plugin support does not create a runtime support
claim. A target requires a committed manifest and lock plus artifact-level
acceptance evidence.

The machine-readable form of a released target is generated from that target's
manifest, lock, and accepted evidence and committed next to them; today that is
`runtime-metadata.windows.json`. Tools should read it instead of this page.

## Runtime capabilities

| Capability | Provider in v0.1.0 | Verified behavior |
| --- | --- | --- |
| `usd` | OpenStrata CY2026 OpenUSD runtime | SDK validation |
| `usd-resolver:http` | `usd-http-resolver` | Cold/warm reads and persistent cache reuse |
| `usd-fileformat:las` | `usd-pointcloud-plugins` | Packaged probe open |
| `usd-fileformat:laz` | `usd-pointcloud-plugins` | Packaged probe open |
| `usd-fileformat:copc` | `usd-pointcloud-plugins` | Local/HTTP reads and validator scenarios |
| `usd-fileformat:ply` | `usd-pointcloud-plugins` | Packaged probe open |
| `usd-fileformat:tif` | `usd-raster-plugins` | GeoTIFF metadata authoring |

Vector formats are not part of the current composition.

## Distribution properties

The released composed runtime is distributed as a content-addressed OCI
artifact with an SPDX SBOM and in-toto provenance. Exact digests belong to the
corresponding [release record](../releases/v0.1.0.md), not this rolling matrix.

The checks behind every verified behavior above are defined in the
[acceptance contract](acceptance-contract.md).

## Claim limits

The committed COPC fixture is synthetic and contains three points. Current
evidence verifies composition, discovery, cache behavior, validator handling,
format reads, and deterministic metadata expectations. It does not establish
large-file performance, broad public-server interoperability, geographic
accuracy, or support for arbitrary files accepted by upstream libraries.
