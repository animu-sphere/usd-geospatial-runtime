# usd-geospatial SDK

A small C++ consumer API over the composed runtime. It does not replace or hide
OpenUSD: `open` returns an ordinary `pxr::UsdStageRefPtr`, and every `pxr::*`
API stays available below it. What the SDK adds is a stable failure vocabulary
and machine-readable introspection, so a consumer — including a future Python
or JavaScript binding — can act on causes instead of parsing text.

The intended surface is described in the [SDK design](../docs/design/sdk.md).
`open`, `runtime_info`, and `formats` are implemented; `inspect` is not, and
joins the API when its contract is proven by tests.

## Two lanes

| Lane | Target | Needs OpenUSD | Provides |
| --- | --- | --- | --- |
| core | `usdgeospatial::core` | no | `Result<T>`, `Diagnostic`, `runtime_info`, `format_support` |
| usd | `usdgeospatial::sdk` | yes | `open`, `formats` |

The split is not cosmetic. Describing a runtime must not require loading it: an
application that finds no runtime, or the wrong one, has to be able to say so,
and `runtime_info` therefore reads the prefix's own
`metadata/composition.lock.json` rather than asking OpenUSD. It is also what
lets CI check the diagnostics, result, and introspection contracts on a runner
with no composed runtime.

`formats` is split along the same line rather than by convenience. Only a
loaded OpenUSD can say which file formats registered, so `formats` itself is in
the USD lane; the rule that turns the composed list and the registered list
into one report is `format_support` in the core lane, where it is tested
against a fixture lock and a fixed registered list per commit.

## Operations

```cpp
#include <usd_geospatial/formats.h>
#include <usd_geospatial/open.h>
#include <usd_geospatial/runtime_info.h>

auto runtime = usd_geospatial::runtime_info(prefix);   // or (), reading USD_GEOSPATIAL_RUNTIME
if (runtime && runtime.value().has_capability("usd-fileformat:copc")) {
    auto stage = usd_geospatial::open(uri);
    if (!stage) {
        report(stage.error().id(), stage.error().detail("capability"));
    }
}
```

`runtime_info` returns the target, the immutable composition identities, the
resolved components, and the capability-to-provider mapping.
`RuntimeInfo::to_json` serializes that as
[`schemas/runtime-info.v1.json`](../schemas/runtime-info.v1.json) — the
introspection subset of the released
[runtime metadata](../schemas/runtime-metadata.v1.json), using the same field
names so a loaded runtime can be compared with a released one field by field.

`formats` answers what this runtime can read. Two lists answer that and they
are not the same question: the composition resolved a set of
`usd-fileformat:` capabilities, and OpenUSD registered the file formats whose
plugins actually loaded. Each extension is therefore reported with the state
that says which of the two claimed it:

```cpp
for (const auto& format : usd_geospatial::formats(runtime.value())) {
    switch (format.state) {
        case usd_geospatial::FormatState::available:   // composed and loaded
        case usd_geospatial::FormatState::not_loaded:  // composed, did not load
        case usd_geospatial::FormatState::undeclared:  // loaded, never composed
            break;
    }
}
```

`not_loaded` is the one worth wiring into a diagnostic: it is a plugin this
composition installed and OpenUSD did not register, and `format.component`
names what to look at. `undeclared` is ordinary — it is how OpenUSD's own
`usda`, `usdc`, and `usdz` appear, since no capability declares them.
Reporting only the composed list, only the registered list, or their
intersection would erase exactly that distinction, so none of the three is what
this returns. `formats_to_json` serializes a report as
[`schemas/formats.v1.json`](../schemas/formats.v1.json).

Failures carry a stable code, a category, the subsystem that observed them, and
structured details. The codes are listed in
[the diagnostics reference](../docs/reference/diagnostics.md).

## Build

The core lane needs only a C++17 compiler:

```powershell
cmake -S sdk -B build/sdk-core -DUSDGEOSPATIAL_BUILD_USD=OFF
cmake --build build/sdk-core --config Release
ctest --test-dir build/sdk-core -C Release --output-on-failure
```

The USD lane needs a composed runtime prefix. `pxr_ROOT` is that prefix, and
the tests derive the runtime they check from it:

```powershell
ost runtime compose runtime-composition.windows.toml `
  --lock runtime.windows.lock.json --locked --output .local/composed
cmake -S sdk -B build/sdk -Dpxr_ROOT=.local/composed -DCMAKE_PREFIX_PATH=.local/composed
cmake --build build/sdk --config Release
python tools/sdk_env.py --composition .local/composed -- `
  ctest --test-dir build/sdk -C Release --output-on-failure
```

Build the SDK with the toolchain its target names — `msvc143` for
`windows-x86_64-msvc143-py313`. The SDK passes standard library types across
the OpenUSD ABI boundary, so a different MSVC toolset is not a supported
configuration even when it links.

### Why `tools/sdk_env.py` and not `ost runtime exec`

`ost runtime exec` replaces the environment with the composition's own search
paths. That total isolation is what makes an acceptance result a claim about
the composition alone, and the acceptance probes run under it because they are
started through the host interpreter, which supplies its own libraries.

A freshly linked native executable cannot. The composed OpenUSD links the host
CPython (`usd_python` against `python313.dll`), and this composition
deliberately does not bundle an interpreter — Python 3.13 is a documented host
prerequisite. Started with an isolated PATH, such an executable fails to load
before it runs a line. `tools/sdk_env.py` prepends the same paths instead of
replacing the environment, which keeps the host interpreter reachable.

Use `ost runtime exec` for anything whose result is a claim about the runtime,
and `tools/sdk_env.py` only to build and test native code against it.

## Consuming the installed SDK

`tests/native-consumer` is a separate CMake project that resolves
`usdGeospatial` and `pxr` from `CMAKE_PREFIX_PATH` the way an external
application does. It compiles against installed public headers only, so an
incomplete install set or a public header that needs a private one fails there
rather than in review:

```powershell
cmake --install build/sdk --config Release --prefix build/sdk-install
cmake -S tests/native-consumer -B build/consumer `
  -DCMAKE_PREFIX_PATH=".local/composed;build/sdk-install"
cmake --build build/consumer --config Release
python tools/sdk_env.py --composition .local/composed -- `
  ctest --test-dir build/consumer -C Release --output-on-failure
```

## Layout

```text
sdk/core/include/usd_geospatial/   public headers, no OpenUSD
sdk/core/src/                      implementation, including a private JSON reader
sdk/core/tests/data/               a synthetic composition lock and the documents it produces
sdk/usd/include/usd_geospatial/    public headers of the operations that need OpenUSD
sdk/usd/src/                       open() and formats()
sdk/cmake/                         package config templates
```

The public API is spelled in snake_case because the conceptual API is shared
across C++, Python, and JavaScript. Types stay PascalCase so `runtime_info` the
function and `RuntimeInfo` the type do not collide.
