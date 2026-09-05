# Testing and Acceptance Design

Status: proposed

## Test layers

| Layer | Owner | Contract |
| --- | --- | --- |
| L1 component | Plugin repositories | Parsing, detection, authoring, diagnostics, and format-specific fixtures. |
| L2 composition | This repository | Discovery, dependency loading, reconstruction, and cross-plugin compatibility. |
| L3 SDK | This repository | Public C++ results, diagnostics, introspection, and stage access. |
| L4 bindings | This repository | Equivalent outcomes across Python, Node, and supported Wasm operations. |
| L5 acceptance | Released artifact | Pull, verify, reconstruct, execute, and inspect from an empty environment. |

Only layers backed by implemented surfaces should be added to CI. The current
repository implements composition and artifact acceptance checks, and the L3
layer for the implemented part of the SDK: its OpenUSD-free lane is tested with
no runtime present, and `open` and `formats` are tested against a composed
prefix. What splits an operation between the two lanes is what it needs, not
convenience -- `formats` joins the composed capability list with the extensions
OpenUSD registered, and that join is tested without a runtime because only
obtaining the second list needs one. Binding layers remain future work.

## Fixture policy

Committed fixtures must be small, redistributable, deterministic, and suitable
for normal CI. Their provenance and limitations must be documented. Synthetic
fixtures establish integration behavior, not production-scale performance,
geographic accuracy, or broad server compatibility.

Real-world and large-file coverage belongs in a separately managed external
corpus used by optional or scheduled compatibility and performance runs.

## Release acceptance

A release candidate must be tested as a consumer receives it:

```text
pull -> verify -> reconstruct -> execute -> open fixture -> inspect result
```

Acceptance must verify artifact and OCI identities, archive safety, expected
artifact kind, file inventory, SBOM, provenance, target metadata, and installed
runtime probes. Evidence output remains outside the immutable runtime prefix.
