# Diagnostic Codes

Every SDK failure carries a code from the table below. A code is the automation
contract: once published, its `UGEO-Ennn` id and its name keep their meaning,
and neither is reused for a different condition. Message text is not a contract
and may change in any release, so branch on the code and read the details.

`tests/tooling/test_metadata_tools.py` compares this table against
`sdk/core/src/diagnostics.cpp`, so the page cannot drift from the source.

## Codes

| Id | Name | Category | Raised when |
| --- | --- | --- | --- |
| `UGEO-E001` | `invalid_argument` | argument | An argument is empty or malformed, so nothing was attempted. |
| `UGEO-E002` | `unsupported_uri_scheme` | argument | No resolver in the composition is registered for the URI scheme. |
| `UGEO-E010` | `runtime_not_specified` | runtime | No prefix was passed and `USD_GEOSPATIAL_RUNTIME` names none. |
| `UGEO-E011` | `runtime_not_found` | runtime | The path is missing, or holds no composition lock, so it is not a composed runtime. |
| `UGEO-E012` | `runtime_metadata_unreadable` | runtime | The composition lock could not be opened, or is not valid JSON. |
| `UGEO-E013` | `runtime_metadata_invalid` | runtime | The lock parsed but does not describe a runtime: no canonical target, no components, or no `usd` capability. |
| `UGEO-E014` | `runtime_schema_unsupported` | runtime | The lock declares a schema this SDK does not read. |
| `UGEO-E030` | `asset_not_found` | asset | The resolver did not resolve the URI to an asset. |
| `UGEO-E031` | `capability_unavailable` | asset | The composition installs no file format for the extension. |
| `UGEO-E032` | `stage_open_failed` | asset | The asset resolved and its format is present, but OpenUSD returned no stage. |

There is deliberately no generic `unknown`: every failure path names its
condition, so a caller can act on the cause rather than parse a message.

## Structure

A diagnostic serializes to a stable JSON object:

```json
{
  "code": "UGEO-E031",
  "name": "capability_unavailable",
  "category": "asset",
  "subsystem": "sdk",
  "message": "this composition installs no file format for the extension",
  "details": {
    "uri": "boundaries.geojson",
    "extension": "geojson",
    "capability": "usd-fileformat:geojson"
  }
}
```

`category` is one of `argument`, `runtime`, or `asset`, for callers that only
need to distinguish a bad call from an unusable runtime from an unusable asset.
`subsystem` is `sdk`, `openstrata`, or `openusd` and says which layer observed
the failure; it is not derivable from the code, because the same condition may
be found by the SDK's own checks or relayed from OpenUSD.

`details` carries the machine-readable specifics that would otherwise exist only
inside the message. The keys are per-code and additive; a caller reads the ones
it knows and ignores the rest. One key is not per-code: `openusd` appears on any
diagnostic the SDK decided for itself while OpenUSD had also posted something,
and carries that text. It is what explains a `UGEO-E031` caused by a plugin that
failed to load rather than by a format that was never composed; `formats()`
reports that same distinction as a `not_loaded` state, without opening an
asset. The `capability` detail on `UGEO-E031` uses the
same capability name the composition manifest and
[runtime metadata](../../schemas/runtime-metadata.v1.json) use, so a consumer
can report exactly which capability a composition would have to add.

## Language bindings

Bindings translate a failure into whatever their language uses, but must
preserve `code`, `name`, `category`, `subsystem`, and `details`. A Python or
JavaScript caller is expected to branch on the same facts a C++ caller does; see
the [SDK design](../design/sdk.md).
