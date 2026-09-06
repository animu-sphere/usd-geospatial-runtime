# Acceptance Contract

This page records the acceptance contract that the repository implements today.
The layered testing intent behind it belongs to the
[testing design](../design/testing.md).

## Required checks

`tools/accept.py` runs only probes installed by the composed artifacts, against
a materialized runtime prefix. Every check below must pass for the run to be
complete; the check names are part of the contract and appear unchanged in both
the acceptance report and the release evidence.

| Check | Proves |
| --- | --- |
| `sdk` | The composition exposes a valid SDK surface for the target. |
| `http` | Cold and warm HTTP reads work and the persistent cache is reused. |
| `pointcloud` | The LAS, LAZ, PLY, and COPC FileFormat plugins open their inputs. |
| `tier2` | Local and HTTP COPC scenarios agree, range reads occur, strong validators produce stable identities, and weak validators do not. |
| `raster` | The GeoTIFF plugin opens a packaged raster and authors the expected metadata. |
| `vector` | The GeoJSON plugin opens a packaged FeatureCollection, preserves its feature identity and properties, and refuses unrelated JSON and malformed GeoJSON. |

A capability added to the composition adds its own check, appended to
`accept.REQUIRED_CHECKS` and given a measurement shape under
[`schemas/release-evidence.v1.json`](../../schemas/release-evidence.v1.json).
Removing or renaming a required check is a contract change: update the schema,
`tools/accept.py`, and this page together.

The required set grows with the composition, so a record states the contract it
was written under in `required_checks`. A record without the field required the
checks listed under `checks.required` in the schema, which is the set the first
v1 release accepted and is never reduced. This is what lets an older record stay
valid, unedited, after a later release adds a capability.

## Generated acceptance output

`tools/accept.py --composition <prefix> --output <dir>` writes each probe's
stdout and stderr plus `summary.json`, which conforms to
[`schemas/acceptance-report.v1.json`](../../schemas/acceptance-report.v1.json).
The runner refuses to write inside the immutable runtime prefix.

`status` is `passed` only when `complete` is true: every required check ran,
exited zero, and passed its derived verification. A check whose probe exits zero
but fails a derived verification records `verification: "failed"` next to its
`exit_code`, so the report stays factual about both. Acceptance output is
generated, is not committed, and is not a release claim on its own.

## Committed release evidence

Release evidence is the concise immutable record of a passing release
acceptance, committed as `evidence/<release>-<slug>.json` — `evidence/v0.1.0-windows.json`
today — and conforming to
[`schemas/release-evidence.v1.json`](../../schemas/release-evidence.v1.json). It
is written only after the published composed artifact has also been pulled into
an empty registry, verified, and reconstructed.

| Evidence field | Meaning |
| --- | --- |
| `release`, `target` | The released version and the canonical composition target. |
| `runtime_digest` | The materialized runtime identity, equal to the committed lock. |
| `composed_artifact`, `composed_oci_manifest` | Identities of the published composed artifact. |
| `clean_public_input_compose` | The composition was reproduced from public immutable inputs in an empty `OST_HOME`. |
| `clean_composed_artifact_reconstruction` | The published artifact was pulled, verified, reconstructed, and re-checked. |
| `required_checks` | The acceptance contract the record was written under. Absent on records that predate the field. |
| `checks` | One entry per acceptance check, carrying the measurements that make the claim auditable. |

A check entry is either a bare status string or an object with `status` plus its
measurements. Checks with measurements to record use the object form; `http`,
`pointcloud`, `tier2`, `raster`, and `vector` are required to do so.

Evidence records are historical. Correct a factual error in a new record or in
an explicit correction, never by rewriting a published one.

## Enforcement

`tools/validate_metadata.py` runs in CI and on release tags. It checks that
every manifest is fully pinned and matches its lock, that every evidence record
is schema valid, records no failed check, and records every check its own
release required, that the generated runtime metadata is committed and current,
that published identities repeated in prose still match, and that the acceptance
runner emits a schema-valid report.

A composition may sit ahead of its evidence between composing a new capability
and releasing it. `tools/runtime_metadata.py` refuses to regenerate that
target's document, because composition identity comes from the lock and
published-artifact identity comes from the evidence, and pairing the two would
publish a runtime identity no accepted release ever carried. The target is
reported as pending and its committed document is checked against the release it
still names.

With `--release <tag>` the gate closes: it requires `VERSION`, a release record,
and evidence for that tag that accepts the committed composition and declares the
current acceptance contract.
