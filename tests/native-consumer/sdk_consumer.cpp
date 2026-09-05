// SPDX-License-Identifier: Apache-2.0
//
// A consumer of the installed SDK.
//
// This file is deliberately outside the SDK build: it includes only installed
// public headers and links only the exported CMake package, so it fails if the
// install set is incomplete, if a public header depends on a private one, or
// if the exported targets do not carry their OpenUSD dependency. That is a
// different claim from the SDK's own tests, which compile inside the build
// tree where all of that is available anyway.
//
// argv[1] is the composed runtime prefix and argv[2] is a readable fixture.

#include <usd_geospatial/diagnostics.h>
#include <usd_geospatial/formats.h>
#include <usd_geospatial/open.h>
#include <usd_geospatial/result.h>
#include <usd_geospatial/runtime_info.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <runtime prefix> <fixture>\n", argv[0]);
        return 2;
    }

    const usd_geospatial::Result<usd_geospatial::RuntimeInfo> runtime =
        usd_geospatial::runtime_info(argv[1]);
    if (!runtime) {
        std::fprintf(stderr, "runtime_info failed: %s\n", runtime.error().to_json().c_str());
        return 1;
    }
    const usd_geospatial::RuntimeInfo& info = runtime.value();
    std::printf("runtime %s %s\n", info.name().c_str(), info.identity().runtime_digest.c_str());
    std::printf("target %s, %zu components, %zu capabilities\n", info.target().id.c_str(),
                info.components().size(), info.capabilities().size());

    // formats() spans both lanes -- the composed half comes from the core
    // library and the registered half from OpenUSD -- so calling it here is
    // what proves the exported package carries both to an outside consumer.
    const std::vector<usd_geospatial::Format> supported = usd_geospatial::formats(info);
    std::size_t usable = 0;
    for (const usd_geospatial::Format& format : supported) {
        usable += format.usable() ? 1 : 0;
    }
    std::printf("%zu formats, %zu usable\n", supported.size(), usable);
    if (usable == 0) {
        std::fprintf(stderr, "the runtime reports no usable file format\n");
        return 1;
    }

    const usd_geospatial::Result<pxr::UsdStageRefPtr> stage = usd_geospatial::open(argv[2]);
    if (!stage) {
        std::fprintf(stderr, "open failed: %s\n", stage.error().to_json().c_str());
        return 1;
    }
    std::printf("opened %s\n", argv[2]);

    // A failure is handled by its code, never by its message: that is the
    // contract a binding or an application is expected to rely on.
    const usd_geospatial::Result<pxr::UsdStageRefPtr> missing =
        usd_geospatial::open("boundaries.geojson");
    if (missing || missing.error().code() != usd_geospatial::DiagnosticCode::capability_unavailable) {
        std::fprintf(stderr, "a missing file format was not reported as a missing capability\n");
        return 1;
    }
    std::printf("missing capability reported as %s (%s)\n", missing.error().id(),
                missing.error().detail("capability").c_str());
    return 0;
}
