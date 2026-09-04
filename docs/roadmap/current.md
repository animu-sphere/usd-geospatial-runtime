# Current Priorities

Status: in progress

The existing v0.1.0 Windows composition is the reference runtime. Work proceeds
in small releases while preserving its locked reconstruction path.

## P0: Baseline the reference runtime

- Define the acceptance contract and stable release-evidence shape.
- Define a machine-readable runtime metadata schema.
- Plan a target-aware manifest and lock layout without breaking v0.1.0 commands.

Completion means a contributor can distinguish immutable release inputs,
generated acceptance output, and the current support claim without inference.

## P1: Add vector capabilities

- Agree capability names with `usd-vector-plugins`.
- Publish immutable provider artifacts.
- Add requirements and provider mappings to the Windows composition.
- Add a small redistributable fixture and installed acceptance probe.
- Publish a release record containing exact input and output identities.

Completion means the reconstructed artifact discovers and exercises the vector
provider through capabilities rather than repository-specific runtime logic.

## P2: Introduce target-aware composition layout

- Move active manifests and locks into a layout that can represent additional
  targets.
- Preserve old release documentation and reconstruction commands.
- Document canonical target identity fields and naming.

Completion means a second target can be added without ambiguous filenames or
changing historical release claims.

## P3: Establish the small C++ SDK

- Define `Result<T>`, diagnostic codes, and `runtime_info` schema.
- Implement `open`, then add `inspect` and `formats` only as their contracts are
  proven by tests.
- Test the public headers from a separate CMake consumer.

Completion means a native consumer can open a supported asset, inspect runtime
identity, and handle failures without parsing message text.

## P4: Prototype Python 3.13 distribution

- Publish the C++ contract through a thin CPython binding.
- Establish `pip install usd-geospatial`, `import usd_geospatial`, and `open`.
- Measure candidate native-runtime delivery models before selecting one.

Completion means a clean supported environment can install, import, and open a
fixture with documented runtime acquisition and diagnostics.
