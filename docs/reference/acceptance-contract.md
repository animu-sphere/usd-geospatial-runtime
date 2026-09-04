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

A capability added to the composition adds its own check. Removing or renaming
a required check is a contract change: update
[`schemas/release-evidence.v1.json`](../../schemas/release-evidence.v1.json),
`tools/accept.py`, and this page in the same change.

## Generated acceptance output

`tools/accept.py --composition <prefix> --output <dir>` writes each probe's
stdout and stderr plus `summary.json`, which conforms to
[`schemas/acceptance-report.v1.json`](../../schemas/acceptance-report.v1.json).
The runner refuses to write inside the immutable runtime prefix.

`status` is `passed` only when `complete` is true: every required check ran,
exited zero, and passed its derived verification. Acceptance output is
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
| `checks` | One entry per acceptance check, carrying the measurements that make the claim auditable. |

A check entry is either a bare status string or an object with `status` plus its
measurements. Checks with measurements to record use the object form; `http`,
`pointcloud`, `tier2`, and `raster` are required to do so.

Evidence records are historical. Correct a factual error in a new record or in
an explicit correction, never by rewriting a published one.

## Enforcement

`tools/validate_metadata.py` runs in CI and on release tags. It checks that
every manifest is fully pinned and matches its lock, that every evidence record
is schema valid and consistent with the lock it claims, that the generated
runtime metadata is committed and current, that published identities repeated in
prose still match, and that the acceptance runner emits a schema-valid report.
With `--release <tag>` it also requires `VERSION`, a release record, and
evidence for that tag.
