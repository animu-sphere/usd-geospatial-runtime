# OST dogfooding reports

This directory contains append-only records of using OpenStrata (`ost`) to
compose, consume, and validate the runtime this repository publishes. Reports
record what a clean consumer or CI runner actually observed, including failures
that are outside this repository's source code.

## Reading order

The newest report carries the current OpenStrata finding and re-enable
conditions. Report 1 records the first hosted OpenUSD SDK lane run and the
producer-specific paths exported by the OpenUSD runtime package.

| # | Date | Report | `ost` | Focus |
| --- | --- | --- | --- | --- |
| 1 | 2026-09-18 | [A hosted SDK lane exposes producer Python paths in the OpenUSD runtime](01-2026-09-18-v0.22.10-openusd-runtime-python-paths.md) | 0.22.10 | `find_package(pxr)` fails on a clean Windows runner; the published CMake package names the producer's Python installation |

Reports are historical evidence. Do not rewrite an old report when a later
`ost` or runtime artifact changes the result; add a new report that rechecks the
finding and points forward.
