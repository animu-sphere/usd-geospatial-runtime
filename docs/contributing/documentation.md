# Documentation Guidelines

Documentation is part of the implementation contract. A change is incomplete
if it changes a support claim, public boundary, implemented architecture, or
delivery status without updating the owning page.

## Category ownership

| Category | Put this here | Do not put this here |
| --- | --- | --- |
| `concepts/` | Stable purpose, vocabulary, principles, and non-goals. | Procedures or implementation inventories. |
| `architecture/` | Current files, dependencies, runtime flow, and verification boundaries. | Unimplemented aspirations. |
| `design/` | Intended contracts, rationale, proposals, and accepted decisions. | Claims that proposed behavior exists. |
| `roadmap/` | Incomplete ordered work and measurable completion conditions. | Completed work or design rationale. |
| `reference/` | Current support matrices and factual contracts. | Plans or unverified portability claims. |
| `releases/` | Immutable versioned scope, identities, evidence, and limitations. | Rolling current-state documentation. |
| `contributing/` | Repository maintenance procedures. | End-user task guides. |

Add `guides/` and `api/` only when implemented user workflows and public APIs
give those categories real content.

## Status rules

Design decisions use `proposed`, `accepted`, `superseded`, or `rejected`.
Roadmap items use `in progress` or `not started`. Architecture and reference
pages describe only implemented facts and do not need proposal statuses.

An accepted decision is historical evidence. Do not substantially rewrite it;
add a new decision that supersedes it and link both documents.

## Links and duplication

- Use relative links within repository documentation.
- Link to the owning document instead of duplicating exact digest tables or
  detailed rationale.
- Keep every category `README.md` synchronized with its files.
- Prefer stable headings because other pages may link to them.
- Wrap commands, paths, targets, package names, and capabilities in code spans.

## Change checklist

1. Confirm planned behavior is not presented as implemented behavior.
2. Confirm each new page appears in its category index and the root index.
3. Check relative links and heading anchors.
4. Update architecture and reference documentation with implementation changes.
5. Remove completed task detail from the roadmap instead of keeping a second
   changelog there.
6. Add immutable release values to a release record and keep rolling reference
   pages free of copied digest tables.
7. Record fixture provenance and state what its evidence cannot prove.
