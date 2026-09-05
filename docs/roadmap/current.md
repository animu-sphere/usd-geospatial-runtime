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

The provider exists. [`usd-vector-plugins`](https://github.com/animu-sphere/usd-vector-plugins)
v0.1.0 publishes the `vector-geojson` plugin bundle as an OpenStrata plugin
product for `cy2026-windows-x86_64-py313-usd`. Its component manifest declares
`usd-fileformat:geojson` and requires only `usd >=26.08,<27.0` and
`usd-stage-read`, both of which the current composition already resolves, so no
capability name has to be negotiated and no new requirement class is introduced.

- Publish the product as an immutable OCI artifact. The v0.1.0 product archive
  digest is `sha256:e89e1e520ad96d16292bc24505cbec8f9eba35165d364a5d897e5e67275fa29b`,
  but no `oci://ghcr.io/animu-sphere/usd-vector-plugins@sha256:...` locator is
  published yet, and this composition pins candidates by OCI digest only.
- Confirm the runtime match before composing. The product provenance records
  verification against OpenUSD runtime artifact `sha256:3a4e3993...`, while this
  composition pins `sha256:51c19df2...` for the same component id. Establish
  that the two are the same runtime, or rebuild and re-verify the product
  against the pinned artifact.
- Ask `usd-vector-plugins` to install an acceptance probe alongside its bundle,
  as the point-cloud and raster products do under
  `share/<component>/probes/`. The published product currently installs only
  `bundles/vector-geojson/...`, and the acceptance runner must keep using
  installed probes rather than gaining repository-specific runtime logic.
- Add the requirement, candidate artifact, and provider mapping to
  `runtime-composition.windows.toml`, recompose with `--locked`, and regenerate
  `runtime-metadata.windows.json`.
- Add a `vector` check to `tools/accept.py`, to the
  [acceptance contract](../reference/acceptance-contract.md), and to the
  required checks in `schemas/release-evidence.v1.json`. The product ships
  `bundles/vector-geojson/tests/fixtures/basic.geojson`, so this repository
  commits a fixture only if the probe needs a repository-owned input.
- Publish a release record containing exact input and output identities, and
  add the capability row to the [support matrix](../reference/support-matrix.md).

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

Its first slice is in place. `Result<T>`, the published diagnostic codes, and
the `runtime_info` schema are defined; `open` and `runtime_info` are
implemented in two lanes under `sdk/`; and `tests/native-consumer` builds
against the installed public headers through the exported CMake package. The
implemented behavior is described in the
[architecture overview](../architecture/overview.md) and the codes in the
[diagnostics reference](../reference/diagnostics.md).

- Add `formats` and `inspect`. Neither has a proven contract yet: `formats`
  needs a decision on whether it reports composed capabilities, OpenUSD's
  registered extensions, or the intersection, and the two differ whenever a
  plugin fails to load. `inspect` needs a defined result before it has a shape.
- Run the SDK lanes in CI. The core lane needs no OpenUSD, so it can build and
  test on the existing runner; the OpenUSD lane needs a composed prefix and a
  matching toolchain, and belongs with acceptance rather than with the
  per-commit checks.
- Decide how the SDK is delivered. It is currently built from source against a
  prefix. Publishing it as an OpenStrata component would let a consumer acquire
  it the way every other component is acquired, and would let `runtime_info`
  discover its own prefix rather than being told one.
- Cover the failure paths the current tests cannot reach: `stage_open_failed`
  needs a malformed asset of a composed format, and
  `runtime_metadata_unreadable` needs an unreadable file rather than a
  malformed string.

Completion means a native consumer can open a supported asset, inspect runtime
identity, and handle failures without parsing message text.

## P3: Prototype Python 3.13 distribution

- Publish the C++ contract through a thin CPython binding.
- Establish `pip install usd-geospatial`, `import usd_geospatial`, and `open`.
- Measure candidate native-runtime delivery models before selecting one.

Completion means a clean supported environment can install, import, and open a
fixture with documented runtime acquisition and diagnostics.
