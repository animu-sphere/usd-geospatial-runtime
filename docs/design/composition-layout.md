# Target-Aware Composition Layout

Status: proposed

## Problem

The repository holds one composition per file at the repository root:
`runtime-composition.windows.toml` and `runtime.windows.lock.json`. The filename
slug (`windows`) is shorter and less precise than the canonical target
(`windows-x86_64-msvc143-py313`), so a second Windows target — a different
toolchain, OpenUSD version, or host ABI — has no unambiguous filename. The
[distribution design](distribution.md) requires each target to carry its own
manifest, lock, evidence, and support statement.

## Canonical target identity

The target string is the identity, and every other target field is derived from
it:

```text
<os>-<arch>-<toolchain>-py<host python>
windows-x86_64-msvc143-py313
```

`tools/runtime_metadata.py` already enforces this shape and rejects a target it
cannot decompose, so an added target cannot silently acquire an ambiguous
identity. The decomposed fields are published in `runtime-metadata.*.json` and
constrained by
[`schemas/runtime-metadata.v1.json`](../../schemas/runtime-metadata.v1.json).

## Proposed layout

```text
targets/<target>/composition.toml
targets/<target>/lock.json
targets/<target>/metadata.json
evidence/<release>-<target>.json
```

Directories are named by the canonical target, so no shortened slug has to stay
unique. Evidence keeps its release-first name because release records, not
targets, own immutable results.

The tooling changes are small: `tools/runtime_metadata.py` discovers manifests
through one glob and derives the lock and metadata paths from the manifest path,
and `tools/validate_metadata.py` iterates whatever discovery returns. Neither
tool hardcodes `windows`.

## Migration constraints

1. Historical instructions stay valid because they are pinned to their tag. The
   v0.1.0 record and its README commands name root paths, and those paths still
   exist at `v0.1.0`. A release record must state the input paths as they were
   at that release rather than tracking later moves.
2. The move must not change the composed identity. Before migrating, recompose
   with `--locked` from the new paths and confirm that `manifest_digest`,
   `composition_digest`, and `runtime_digest` are unchanged. If OpenStrata
   derives any of them from the manifest filename or path, the layout change
   becomes a new release rather than a relocation.
3. `runtime-metadata.*.json` is generated. After the move it is regenerated in
   the same commit, and `generated_by.inputs` records the new paths.
4. One commit performs the move and the tooling update together so no
   intermediate commit has a lock that its validator cannot find.

## Alternatives considered

Keeping the flat layout with fully qualified filenames
(`runtime-composition.windows-x86_64-msvc143-py313.toml`) also removes the
ambiguity and needs no tooling change beyond the glob. It is rejected because
each target grows more sibling files — manifest, lock, metadata, and any future
per-target inputs — and a directory keeps those together.

## Status and sequencing

This layout is not implemented. It is scheduled after vector capabilities are
composed, so that the first migration moves a composition whose contents are
already settled.
