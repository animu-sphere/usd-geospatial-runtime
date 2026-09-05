# USD Geospatial Runtime Documentation

The documentation separates the runtime that exists today from its intended
consumer surface and from the work required to get there. Start with the
category that matches the question you are trying to answer.

| Question | Source of truth | Start here |
| --- | --- | --- |
| What is this project and which principles define it? | [Concepts](concepts/) | [Project overview](concepts/overview.md) |
| What exists on the default branch today? | [Architecture](architecture/) | [Current architecture](architecture/overview.md) |
| What are the intended product and API contracts? | [Design](design/) | [Design specification](design/spec.md) |
| What should be implemented next, and in what order? | [Roadmap](roadmap/) | [Current priorities](roadmap/current.md) |
| Which targets, capabilities, and formats are supported? | [Reference](reference/) | [Support matrix](reference/support-matrix.md) |
| What was shipped in a particular version? | [Releases](releases/) | [Release records](releases/README.md) |
| What is the machine-readable contract for tooling? | [Schemas](../schemas/) | [Schema index](../schemas/README.md) |
| How should contributors maintain these documents? | [Contributing](contributing/) | [Documentation guidelines](contributing/documentation.md) |

## Reading paths

For an implementation overview, read the
[current architecture](architecture/overview.md), then the
[support matrix](reference/support-matrix.md).

For future implementation work, begin with the
[design specification](design/spec.md), open the focused contract for the area
being changed, and then consult the [current priorities](roadmap/current.md).

## Source-of-truth policy

- Repository files are authoritative for implemented behavior. The
  [architecture documentation](architecture/) records that behavior and must be
  updated with implementation changes.
- The [design documentation](design/) defines intended contracts. It may
  describe systems that do not exist yet, but must label that status clearly.
- The [roadmap](roadmap/) contains incomplete delivery work only. Completed
  behavior moves to architecture or reference documentation.
- The [reference documentation](reference/) records current factual support
  claims derived from manifests, locks, and release evidence.
- [Release records](releases/) are immutable historical summaries. Correct a
  factual error explicitly rather than silently rewriting release history.

The C++ SDK's own build, test, and consumption instructions live next to it in
[sdk/README.md](../sdk/README.md). Guides and a generated API reference will be
added when an implemented end-user binding gives those categories real content.
