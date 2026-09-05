// SPDX-License-Identifier: Apache-2.0
//
// usdGeospatial open() and formats() tests.
//
// These need a loaded runtime, so they run inside a composed prefix -- see
// sdk/README.md. Like the core tests they use no framework: a failed check
// aborts, which is what CTest reads.
//
// argv[1] is the composed runtime prefix, argv[2] is a readable point-cloud
// fixture, and argv[3] is a deliberately malformed asset of a format OpenUSD
// dispatches; CMake passes all three.

#include <usd_geospatial/formats.h>
#include <usd_geospatial/open.h>
#include <usd_geospatial/runtime_info.h>

#include <pxr/usd/usd/prim.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

int g_checks = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        std::abort();
    }
}

#define CHECK(expr) Check((expr), #expr)

using namespace usd_geospatial;

void report(const char* label, const Diagnostic& diagnostic) {
    std::printf("%s -> %s\n", label, diagnostic.to_json().c_str());
}

/// Nothing is attempted for an argument that cannot name an asset, and the
/// caller is told which of the two reasons applied.
void test_rejects_unusable_arguments() {
    Result<pxr::UsdStageRefPtr> empty = open("");
    CHECK(!empty.ok());
    CHECK(empty.error().code() == DiagnosticCode::invalid_argument);

    Result<pxr::UsdStageRefPtr> bare = open("a-name-with-no-extension");
    CHECK(!bare.ok());
    CHECK(bare.error().code() == DiagnosticCode::invalid_argument);
    report("no extension", bare.error());
}

/// A scheme no composed resolver claims is its own condition, separate from
/// "the asset is missing": nothing was ever asked to fetch it.
void test_reports_an_unhandled_scheme() {
    Result<pxr::UsdStageRefPtr> result = open("ftp://example.invalid/site.usda");
    CHECK(!result.ok());
    CHECK(result.error().code() == DiagnosticCode::unsupported_uri_scheme);
    CHECK(result.error().detail("scheme") == "ftp");
    // The composition provides the HTTP resolver, so its schemes are the
    // evidence that the registered list is real and not empty.
    CHECK(result.error().detail("registered").find("http") != std::string::npos);
    report("unhandled scheme", result.error());
}

/// A missing format is reported as the capability a composition would have to
/// add, using the same capability name the manifest and metadata use. GeoJSON
/// is the honest example: docs/roadmap/current.md has it as pending work, so
/// this composition genuinely cannot open one.
void test_reports_a_missing_capability() {
    Result<pxr::UsdStageRefPtr> result = open("boundaries.geojson");
    CHECK(!result.ok());
    CHECK(result.error().code() == DiagnosticCode::capability_unavailable);
    CHECK(result.error().detail("extension") == "geojson");
    CHECK(result.error().detail("capability") == "usd-fileformat:geojson");
    report("missing capability", result.error());
}

/// A format that is installed but an asset that is not present is a different
/// condition again, and the one a caller retries or reports as a bad path.
void test_reports_a_missing_asset() {
    Result<pxr::UsdStageRefPtr> result = open("no-such-asset-8f31c2.usda");
    CHECK(!result.ok());
    CHECK(result.error().code() == DiagnosticCode::asset_not_found);
    report("missing asset", result.error());
}

/// The last condition open() separates: the asset resolved, its file format is
/// present, and OpenUSD still declined. Reaching it needs a malformed asset of
/// a format OpenUSD dispatches -- `usda`, which the composed OpenUSD provides
/// itself rather than through a capability -- because every earlier check has
/// to pass first.
void test_reports_a_malformed_asset(const std::string& malformed) {
    Result<pxr::UsdStageRefPtr> result = open(malformed);
    CHECK(!result.ok());
    CHECK(result.error().code() == DiagnosticCode::stage_open_failed);
    CHECK(result.error().detail("extension") == "usda");
    // The condition is the SDK's, but the explanation belongs to OpenUSD and is
    // relayed rather than replaced -- and the subsystem says so, which is what
    // tells a caller triaging a bug which layer to look at.
    CHECK(result.error().subsystem() == Subsystem::openusd);
    CHECK(!result.error().message().empty());
    report("malformed asset", result.error());
}

void test_opens_a_composed_format(const std::string& fixture) {
    Result<pxr::UsdStageRefPtr> result = open(fixture);
    if (!result.ok()) {
        std::fprintf(stderr, "FAILED: %s\n", result.error().to_json().c_str());
        std::abort();
    }
    // The value is an ordinary OpenUSD stage, not a wrapper: the SDK adds a
    // typed failure path and gets out of the way on success.
    const pxr::UsdStageRefPtr stage = result.take();
    CHECK(static_cast<bool>(stage));
    CHECK(static_cast<bool>(stage->GetPseudoRoot()));
}

/// The runtime describes itself from the prefix that is actually present, and
/// the capability it reports is the one open() just used.
void test_runtime_info_matches_the_prefix(const std::string& prefix) {
    Result<RuntimeInfo> result = runtime_info(prefix);
    if (!result.ok()) {
        std::fprintf(stderr, "FAILED: %s\n", result.error().to_json().c_str());
        std::abort();
    }
    const RuntimeInfo& info = result.value();
    // The target is asserted by shape, not by value, so this test stays true
    // when a second target is composed.
    const Target& target = info.target();
    CHECK(info.name() == "usd-geospatial-runtime");
    CHECK(target.id == target.os + "-" + target.arch + "-" + target.toolchain + "-" + target.host_abi);
    CHECK(!target.usd_version.empty());
    CHECK(info.has_capability("usd"));
    CHECK(info.has_capability("usd-fileformat:copc"));
    CHECK(!info.has_capability("usd-fileformat:geojson"));
    CHECK(info.identity().runtime_digest.rfind("sha256:", 0) == 0);
    std::printf("%s\n", info.to_json().c_str());
}

/// formats() joins what the composition resolved with what OpenUSD dispatches.
/// In a healthy runtime the two agree on every composed format, and asserting
/// that they do is what this test is for: a plugin that ships and does not load
/// passes every other test in this file, because nothing else here opens an
/// asset of the format it was supposed to provide.
void test_formats_report(const std::string& prefix) {
    Result<RuntimeInfo> runtime = runtime_info(prefix);
    if (!runtime.ok()) {
        std::fprintf(stderr, "FAILED: %s\n", runtime.error().to_json().c_str());
        std::abort();
    }
    const std::vector<Format> supported = formats(runtime.value());
    CHECK(!supported.empty());

    // Counted as extensions, not as capabilities: two capabilities that
    // normalize to one extension are one format in the report, and comparing
    // against a capability count would fail on the report being right.
    std::set<std::string> declared;
    for (const Capability& capability : runtime.value().capabilities()) {
        const std::string extension = format_extension(capability.name);
        if (!extension.empty()) {
            declared.insert(extension);
        }
    }
    CHECK(!declared.empty());

    std::size_t composed = 0;
    bool copc = false;
    bool own = false;
    for (const Format& format : supported) {
        if (format.composed()) {
            ++composed;
            CHECK(format.capability == format_capability(format.extension));
            CHECK(!format.component.empty());
            // Every format this composition resolved must also have loaded.
            // This is a real assertion because registered_extensions() asks
            // OpenUSD for each format rather than reading the plugInfo index,
            // so a plugin that ships and fails to load fails here. The
            // capability name is the failure message: it names what to look at.
            Check(format.state == FormatState::available, format.capability.c_str());
            copc = copc || format.extension == "copc";
        } else {
            // OpenUSD's own formats were registered and never declared. They
            // are reported rather than hidden: a caller asking what it can open
            // needs to know they work.
            CHECK(format.component.empty());
            own = own || format.extension == "usda";
        }
    }
    CHECK(composed == declared.size());
    CHECK(copc);
    CHECK(own);

    // The no-argument overload answers for the runtime the environment names.
    // Whether one is named is a property of how this test was launched, so what
    // is asserted is that it never invents an empty report: it either answers
    // or fails with a runtime condition.
    Result<std::vector<Format>> ambient = formats();
    if (ambient) {
        CHECK(!ambient.value().empty());
    } else {
        CHECK(ambient.error().category() == Category::runtime);
    }

    std::printf("%s\n", formats_to_json(supported).c_str());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <runtime prefix> <point cloud fixture> <malformed asset>\n",
                     argv[0]);
        return 2;
    }
    const std::string prefix = argv[1];
    const std::string fixture = argv[2];
    const std::string malformed = argv[3];

    test_rejects_unusable_arguments();
    test_reports_an_unhandled_scheme();
    test_reports_a_missing_capability();
    test_reports_a_missing_asset();
    test_reports_a_malformed_asset(malformed);
    test_opens_a_composed_format(fixture);
    test_runtime_info_matches_the_prefix(prefix);
    test_formats_report(prefix);

    std::printf("usdGeospatial: %d checks passed\n", g_checks);
    return 0;
}
