# Fixtures

## Synthetic COPC input

`three-points.copc`: 814 bytes,
SHA-256 `422aa1af2d74323bb73e696c50b3cf9c7147e568c6a339ae3573095588f65dd5`.

Generated on 2026-08-26 by the `pointcloudCopc_integration` test in
usd-pointcloud-plugins, source revision
`5695332c311db7a2d3199b7c02a7afc407b169e9`:
`plugins/pointcloud-copc/tests/test_pointcloud_copc.cpp`,
`WriteEquivalentCopc` / `WriteEquivalentLaz` (including the changed-record case).
The fixture was captured after that test passed against OpenUSD 26.08.
It contains three synthetic point records; no survey data or personal data.
The generator is Apache-2.0; its vendored laz-perf encoder is Apache-2.0.

This fixture is an integration input, not a real-world throughput benchmark.

## Malformed asset

`malformed.usda`: 92 bytes, SHA-256 `abca3450000b68ceec20fc005575ed86330d99248def04c1d4b1a5e83f9ed72d`.

A `.usda` file whose layer metadata block opens with an unterminated string, so
OpenUSD recognizes the format and then fails to compose a stage from it. That is
the only way to reach `UGEO-E032` (`stage_open_failed`), which is the condition
`open` reports when the asset resolved and its file format is present but
OpenUSD declined: every earlier check has to pass first. `usda` is used rather
than a truncated point cloud so the failure comes from OpenUSD's own parser and
does not depend on how one plugin handles damaged input.

It is committed deliberately broken and is never a valid input. `.gitattributes`
keeps it LF in every working tree so the digest above stays true.
