# Current Priorities

Status: in progress

The existing v0.1.0 Windows composition is the reference runtime. Work proceeds
in small releases while preserving its locked reconstruction path.

Its baseline is in place: the required acceptance checks and the committed
evidence shape are recorded in the
[acceptance contract](../reference/acceptance-contract.md), runtime metadata is
generated and schema checked, and the target-aware layout is planned in the
[composition layout design](../design/composition-layout.md).

## P0: Compose the vector capability

Blocked on the provider, not on this repository. Composing
[`usd-vector-plugins`](https://github.com/animu-sphere/usd-vector-plugins) v0.1.0
was attempted and withdrawn: the published product was not built against the
runtime this composition pins, so accepting it would break the rule that a
capability enters the composition only when it was verified against the runtime
it will be loaded into.

The composition records, for every component, the runtime identity it was built
against. Seven of eight agree with the pinned OpenUSD runtime
`sha256:51c19df2...`, whose own identity is `sha256:3a4e3993...`. The vector
product records `sha256:ce996432...`, which is not the identity of any artifact
this composition knows. Its bundle says where that came from: the installed
`bundles/vector-geojson/validation/environment.json` names a hand-placed
`C:/usd/openusd-26.08-cy2026` tree, while every other composed product names the
managed runtime store. That the plugin loads and passes its probe here shows
only that the two were ABI-compatible on one machine.

The provider's own configuration is already correct, so this is a release-process
defect rather than a design problem. `usd-vector-plugins` pins runtime artifact
`sha256:ebb0c7da...` in `openstrata.ci.yaml` and its release workflow, and that
artifact's identity is `sha256:3a4e3993...` -- the same runtime, by a different
archive. The published artifact was simply not produced by that path.

The other two concerns this item once raised are closed and need no further work:

- The product is published as an immutable OCI artifact. A pull verified the OCI
  digest, archive digest, per-file digests, artifact kind, and SBOM.
- It installs an acceptance probe at
  `share/usd-vector-plugins/probes/packaged_probe.py`, taking `--prefix` like the
  point-cloud and raster probes, so the acceptance runner keeps using only
  installed probes.

What this repository does when a conforming product is published:

- Add the requirement, candidate artifact, and provider mapping to
  `runtime-composition.windows.toml`, recompose, and confirm the product manifest
  records runtime identity `sha256:3a4e3993...` before pinning it.
- Add a `vector` check to `tools/accept.py`, to the
  [acceptance contract](../reference/acceptance-contract.md), and to
  `schemas/release-evidence.v1.json`. Record the refusal count as well as the
  feature count: what distinguishes GeoJSON detection is that it declines
  unrelated `.json`, so a record that only says the fixture opened is not
  auditable.
- Release it. Adding a capability moves the composition ahead of its evidence,
  and two pieces of tooling assume that never happens: every evidence record is
  required to match the current lock, which would invalidate v0.1.0's record, and
  `tools/runtime_metadata.py` takes the newest evidence for the target
  unconditionally, which would pair the new composition identity with v0.1.0's
  published artifact identity. Both need fixing in the same change, alongside a
  way for a record to state the contract it was written under so a historical
  record stays valid when the required set grows.

Completion means the reconstructed artifact opens a GeoJSON asset through
capability discovery, and its release evidence records a passing `vector` check
next to the existing ones.

## P1: Introduce target-aware composition layout

- Accept the proposed layout, then move active manifests, locks, and generated
  metadata into it.
- Confirm the move leaves the manifest, composition, and runtime digests
  unchanged before treating it as a relocation rather than a release.
- Preserve old release documentation and reconstruction commands.

Completion means a second target can be added without ambiguous filenames or
changing historical release claims.

## P2: Establish the small C++ SDK

Most of it is in place. `Result<T>`, the published diagnostic codes, the
`runtime_info` and `formats` schemas, and the `open`, `runtime_info`, and
`formats` operations are implemented in two lanes under `sdk/`, and
`tests/native-consumer` builds against the installed public headers through the
exported CMake package. The implemented behavior is described in the
[architecture overview](../architecture/overview.md) and the codes in the
[diagnostics reference](../reference/diagnostics.md).

`formats` settled the open question by refusing the choice: it reports neither
the composed capabilities, nor OpenUSD's registered extensions, nor their
intersection, but every extension with the state that says which source claimed
it. The two lists differ exactly when a plugin fails to load, so collapsing
them would erase the case the operation exists for. The join is a pure function
in the OpenUSD-free lane and is tested per commit; only the registered list
needs a loaded runtime.

- Add `inspect`. It still has no proven contract: it needs a defined result --
  what it reports for an asset that opens, and what it reports for one that
  does not -- before it has a shape.
- Run the OpenUSD lane in CI. The core lane already builds and tests per
  commit; the OpenUSD lane needs a composed prefix and a toolchain matching the
  target, so it belongs with acceptance rather than with the per-commit checks
  and has no runner yet. Its tests now cover `formats` and every `open` failure
  path, but none of that is proven until that runner exists.
- Decide how the SDK is delivered. It is currently built from source against a
  prefix. Publishing it as an OpenStrata component would let a consumer acquire
  it the way every other component is acquired, and would let `runtime_info`
  discover its own prefix rather than being told one.

Completion means a native consumer can open a supported asset, inspect runtime
identity, and handle failures without parsing message text.

## P3: Prototype Python 3.13 distribution

- Publish the C++ contract through a thin CPython binding.
- Establish `pip install usd-geospatial`, `import usd_geospatial`, and `open`.
- Measure candidate native-runtime delivery models before selecting one.

Completion means a clean supported environment can install, import, and open a
fixture with documented runtime acquisition and diagnostics.
