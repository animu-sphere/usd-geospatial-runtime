# Distribution Design

Status: proposed

## Sources of truth

```text
source and manifests  -> GitHub
native runtime        -> OCI / GHCR
Python entry point    -> PyPI
JavaScript entry point-> npm
documentation         -> repository or documentation site
```

OCI remains the source of truth for native composed runtimes because it
supports immutable digests, provenance, SBOMs, registry distribution, and the
existing OpenStrata lifecycle. PyPI and npm are ecosystem entry points, not the
canonical store for every runtime binary.

## Candidate package models

The Python and JavaScript prototypes should compare:

- embedding the target runtime in the package;
- platform-specific companion packages;
- OCI acquisition with a local content-addressed cache; and
- discovery of an externally installed runtime.

The selection must be based on download size, installed size, cold install
time, startup time, cache reuse, offline behavior, and CI usability. Repository
commits must not contain generated package binaries.

## Target progression

Windows x86_64 with MSVC 14.3 and Python 3.13 is the reference target. The
intended progression is Linux x86_64, macOS arm64, Python package UX, Node
native support, and then a scoped Wasm experiment. Each step requires its own
manifest, lock, acceptance evidence, and support statement.

The current top-level Windows manifest and lock may move into target-aware
directories only through a migration that preserves v0.1.0 reconstruction
instructions and immutable release records.
