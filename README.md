# USD Geospatial Runtime

An immutable OpenStrata composition of OpenUSD 26.08, the HTTP resolver,
point-cloud FileFormat plugins, and the GeoTIFF FileFormat plugin.

## v0.1.0

The first release targets Windows x86_64, MSVC 14.3, and host Python 3.13. The
composition manifest and lock pin seven independently published component
artifacts by both their OpenStrata archive digest and OCI manifest digest.

- Runtime identity: `sha256:904b8c12be5668fde17eda3f49c22b5cc30a253a5ffd3cf851d136ea7457f6bc`
- Composed artifact: `sha256:e0bc2fb1c6da23e8e3c722eefd6e6b0623aa7c0d0679e41e30a82152e61e1c62`
- OCI: `oci://ghcr.io/animu-sphere/usd-geospatial-runtime@sha256:182e5382cd5096a1f223f6ea65fbd1460d2fc249d68accf6e9f83281404ec3f3`

The composed artifact carries an SPDX SBOM and in-toto provenance. A pull into
an empty OST registry verified the OCI digest, archive digest, file inventory,
SBOM, provenance, archive safety, and artifact kind before reconstruction.

## Reproduce the composition

OpenStrata v0.22.8 or newer is required.

```powershell
ost runtime compose runtime-composition.windows.toml `
  --lock runtime.windows.lock.json --locked --output .local/composed
python tools/accept.py --composition .local/composed `
  --output .local/evidence
```

To consume the already composed artifact:

```powershell
ost artifact pull `
  oci://ghcr.io/animu-sphere/usd-geospatial-runtime@sha256:182e5382cd5096a1f223f6ea65fbd1460d2fc249d68accf6e9f83281404ec3f3 `
  --expect-artifact sha256:e0bc2fb1c6da23e8e3c722eefd6e6b0623aa7c0d0679e41e30a82152e61e1c62 `
  --require-kind composed-runtime
ost runtime reconstruct `
  --from-artifact sha256:e0bc2fb1c6da23e8e3c722eefd6e6b0623aa7c0d0679e41e30a82152e61e1c62 `
  --output .local/reconstructed
```

The acceptance runner uses only probes installed in the composition. It checks
the SDK, HTTP cold/warm cache behavior, LAS/LAZ/PLY/COPC reads, ten HTTP/COPC
validator scenarios, and GeoTIFF metadata authoring. Python 3.13 is an explicit
host prerequisite and is not bundled.

## Inspect and validate

`runtime-metadata.windows.json` describes the released target in machine-readable
form: canonical target identity, immutable composition and artifact identities,
component versions, and the component that provides each capability. It is
generated from the manifest, the lock, and the accepted release evidence, and it
is validated against the schemas in `schemas/`.

```powershell
python tools/runtime_metadata.py --write
python tools/validate_metadata.py
python tests/tooling/test_metadata_tools.py
```

These commands run in CI and need only CPython 3.13; the repository installs no
Python packages.

## Scope limits

This release makes no Linux or macOS composition claim. Its synthetic fixtures
prove integration, cache reuse, validator invalidation, and format discovery;
they do not establish large-file performance, broad server interoperability,
or geographic accuracy.

See [the v0.1.0 release record](docs/releases/v0.1.0.md) for exact inputs and
acceptance results.
