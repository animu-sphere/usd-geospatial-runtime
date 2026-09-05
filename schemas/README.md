# Schemas

JSON Schema (draft 2020-12) definitions for the machine-readable documents this
repository produces. They are the automation contract: field names and shapes
here are stable, and human-readable text is not a contract.

| Schema | Describes | Instances |
| --- | --- | --- |
| [runtime-metadata.v1.json](runtime-metadata.v1.json) | One composed target: canonical target identity, immutable identities, components, and capabilities. | `runtime-metadata.*.json` |
| [release-evidence.v1.json](release-evidence.v1.json) | The immutable acceptance result committed for a released target. | `evidence/*.json` |
| [acceptance-report.v1.json](acceptance-report.v1.json) | The generated report written by `tools/accept.py` for a composed prefix. | Acceptance output, not committed |
| [runtime-info.v1.json](runtime-info.v1.json) | What the SDK reports about the runtime it found: the introspection subset of runtime metadata. | `usd_geospatial::RuntimeInfo::to_json` output |
| [formats.v1.json](formats.v1.json) | Every file extension a runtime knows about and which of the composition and OpenUSD claimed it. | `usd_geospatial::formats_to_json` output |

## Validation

`tools/validate_metadata.py` validates every committed instance, regenerates the
runtime metadata to prove it is not stale, and checks that the identities
repeated in prose still match. It runs in CI and needs only CPython 3.13:

```powershell
python tools/validate_metadata.py
python tests/tooling/test_metadata_tools.py
```

The runtime-info and formats schemas are enforced from both sides.
`sdk/core/tests/data/runtime-info.json` and `sdk/core/tests/data/formats.json`
are the documents the SDK is asserted to produce, and the tooling tests validate
those same files against their schemas, so the C++ serializers and the schemas
cannot drift apart.

A formats document depends on which plugins loaded, which no committed file can
capture. The committed one is produced by the core lane from a fixture lock and
a fixed registered-extension list, so it is reproducible and still exercises
every state -- including the composed format that did not load.

Validation uses `tools/jsonschema_lite.py`, a small validator limited to the
keyword subset these schemas use. It walks the whole schema before validating,
including branches no instance reaches, and raises on any unsupported keyword,
so a schema cannot silently stop being enforced. Add the keyword to that module
before using it in a schema.

## Versioning

A schema file is immutable once a release depends on it. Additive optional
fields may be introduced in place; any change that would reject a previously
valid document requires a new `*.vN.json` file, and the documents that move to
it must be regenerated. Released evidence keeps the schema version it was
written with.
