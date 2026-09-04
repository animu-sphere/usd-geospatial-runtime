# Schemas

JSON Schema (draft 2020-12) definitions for the machine-readable documents this
repository produces. They are the automation contract: field names and shapes
here are stable, and human-readable text is not a contract.

| Schema | Describes | Instances |
| --- | --- | --- |
| [runtime-metadata.v1.json](runtime-metadata.v1.json) | One composed target: canonical target identity, immutable identities, components, and capabilities. | `runtime-metadata.*.json` |
| [release-evidence.v1.json](release-evidence.v1.json) | The immutable acceptance result committed for a released target. | `evidence/*.json` |
| [acceptance-report.v1.json](acceptance-report.v1.json) | The generated report written by `tools/accept.py` for a composed prefix. | Acceptance output, not committed |

## Validation

`tools/validate_metadata.py` validates every committed instance, regenerates the
runtime metadata to prove it is not stale, and checks that the identities
repeated in prose still match. It runs in CI and needs only CPython 3.13:

```powershell
python tools/validate_metadata.py
python tests/tooling/test_metadata_tools.py
```

Validation uses `tools/jsonschema_lite.py`, a small validator limited to the
keyword subset these schemas use. It raises on any unsupported keyword, so a
schema cannot silently stop being enforced. Add the keyword to that module
before using it in a schema.

## Versioning

A schema file is immutable once a release depends on it. Additive optional
fields may be introduced in place; any change that would reject a previously
valid document requires a new `*.vN.json` file, and the documents that move to
it must be regenerated. Released evidence keeps the schema version it was
written with.
