# Current Priorities

Status: in progress

The existing v0.1.0 Windows composition is the reference runtime. Work proceeds
in small releases while preserving its locked reconstruction path.

Its baseline is in place: the required acceptance checks and the committed
evidence shape are recorded in the
[acceptance contract](../reference/acceptance-contract.md), runtime metadata is
generated and schema checked, and the target-aware layout is planned in the
[composition layout design](../design/composition-layout.md).

## P0: Release the composed vector capability

The capability is composed. [`usd-vector-plugins`](https://github.com/animu-sphere/usd-vector-plugins)
v0.1.0 is pinned as an eighth candidate artifact by archive digest
`sha256:153c20f3...` and OCI manifest digest `sha256:acfbe867...`, it provides
`usd-fileformat:geojson`, and the recomposed target resolves to runtime
`sha256:562444f4...`. Its packaged probe runs as the required `vector`
acceptance check, and a local run of `tools/accept.py` passes all six checks.

The three concerns this item opened with are closed. The product is published as
an immutable OCI artifact. The runtime match is established by identity rather
than by argument: `usd-vector-plugins`, `usd-pointcloud-plugins`, and
`usd-http-resolver` all record the same runtime digest
`sha256:3a4e3993...` in their `strata.lock`, and the latter two are already
composed here. The product now installs its probe under
`share/usd-vector-plugins/probes/`, like the point-cloud and raster products, so
the acceptance runner still uses only installed probes.

What remains is the release, which is the only step that can turn the
composition into a support claim:

- Push the composed runtime to
  `oci://ghcr.io/animu-sphere/usd-geospatial-runtime` and record the composed
  artifact and OCI manifest digests.
- Reproduce the composition from public inputs in an empty `OST_HOME`, then pull,
  verify, and reconstruct the published artifact and re-run acceptance against
  it.
- Commit `evidence/v0.2.0-windows.json` with `required_checks` naming the six
  current checks, write `docs/releases/v0.2.0.md`, bump `VERSION`, regenerate
  `runtime-metadata.windows.json`, and update the identities the README
  publishes.
- Move `usd-fileformat:geojson` into the
  [support matrix](../reference/support-matrix.md) capability table.

Until then `tools/runtime_metadata.py` reports the target as awaiting release
acceptance and leaves the committed document describing v0.1.0, and
`tools/validate_metadata.py --release` refuses a tag whose evidence does not
accept the committed composition.

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
