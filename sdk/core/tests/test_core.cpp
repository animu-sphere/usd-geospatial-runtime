// SPDX-License-Identifier: Apache-2.0
//
// usdGeospatialCore unit tests.
//
// No test framework: the core lane must configure, build, and run on a machine
// with no OpenUSD and no package manager, so the assertions are a function and
// an abort. A failure aborts with a non-zero status, which is what CTest reads.
//
// The test data directory is argv[1]; CMake passes it.

#include <usd_geospatial/diagnostics.h>
#include <usd_geospatial/format_support.h>
#include <usd_geospatial/result.h>
#include <usd_geospatial/runtime_info.h>

#include "json.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
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

std::string g_data;

std::string read(const std::string& name) {
    std::ifstream file(g_data + "/" + name, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "FAILED: test data %s is missing\n", name.c_str());
        std::abort();
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    // .gitattributes keeps these files LF in every working tree, but a copy
    // that arrived some other way must not turn a real comparison into a
    // line-ending comparison.
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    // Committed files end with a newline; the serialized documents do not.
    if (!text.empty() && text.back() == '\n') {
        text.pop_back();
    }
    return text;
}

using namespace usd_geospatial;

// Every published code keeps its id and its name forever, so the tables are
// asserted rather than trusted: a renumbering that would break a caller's
// branch fails here first.
void test_diagnostic_codes() {
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::invalid_argument)) == "UGEO-E001");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::unsupported_uri_scheme)) == "UGEO-E002");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::runtime_not_specified)) == "UGEO-E010");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::runtime_not_found)) == "UGEO-E011");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::runtime_metadata_unreadable)) == "UGEO-E012");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::runtime_metadata_invalid)) == "UGEO-E013");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::runtime_schema_unsupported)) == "UGEO-E014");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::asset_not_found)) == "UGEO-E030");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::capability_unavailable)) == "UGEO-E031");
    CHECK(std::string(diagnostic_code_id(DiagnosticCode::stage_open_failed)) == "UGEO-E032");

    CHECK(std::string(diagnostic_code_name(DiagnosticCode::runtime_schema_unsupported)) ==
          "runtime_schema_unsupported");
    CHECK(diagnostic_category(DiagnosticCode::invalid_argument) == Category::argument);
    CHECK(diagnostic_category(DiagnosticCode::runtime_not_found) == Category::runtime);
    CHECK(diagnostic_category(DiagnosticCode::stage_open_failed) == Category::asset);
    CHECK(std::string(category_name(Category::asset)) == "asset");
    CHECK(std::string(subsystem_name(Subsystem::openusd)) == "openusd");

    // No two codes may share an id or a name.
    const DiagnosticCode all[] = {
        DiagnosticCode::invalid_argument,           DiagnosticCode::unsupported_uri_scheme,
        DiagnosticCode::runtime_not_specified,      DiagnosticCode::runtime_not_found,
        DiagnosticCode::runtime_metadata_unreadable, DiagnosticCode::runtime_metadata_invalid,
        DiagnosticCode::runtime_schema_unsupported, DiagnosticCode::asset_not_found,
        DiagnosticCode::capability_unavailable,     DiagnosticCode::stage_open_failed,
    };
    std::set<std::string> ids;
    std::set<std::string> names;
    for (const DiagnosticCode code : all) {
        ids.insert(diagnostic_code_id(code));
        names.insert(diagnostic_code_name(code));
    }
    CHECK(ids.size() == sizeof(all) / sizeof(all[0]));
    CHECK(names.size() == sizeof(all) / sizeof(all[0]));
}

void test_diagnostic_details() {
    Diagnostic diagnostic(DiagnosticCode::capability_unavailable, Subsystem::sdk,
                          "this composition installs no file format for the extension");
    diagnostic.with("extension", "geojson").with("capability", "usd-fileformat:geojson");

    CHECK(diagnostic.details().size() == 2);
    CHECK(diagnostic.detail("capability") == "usd-fileformat:geojson");
    CHECK(diagnostic.detail("absent").empty());
    CHECK(diagnostic.to_json() ==
          "{\n"
          "  \"code\": \"UGEO-E031\",\n"
          "  \"name\": \"capability_unavailable\",\n"
          "  \"category\": \"asset\",\n"
          "  \"subsystem\": \"sdk\",\n"
          "  \"message\": \"this composition installs no file format for the extension\",\n"
          "  \"details\": {\n"
          "    \"extension\": \"geojson\",\n"
          "    \"capability\": \"usd-fileformat:geojson\"\n"
          "  }\n"
          "}");

    const Diagnostic empty(DiagnosticCode::invalid_argument, Subsystem::sdk, "no details");
    CHECK(empty.to_json().find("\"details\": {}") != std::string::npos);
}

void test_result() {
    Result<std::string> ok = Result<std::string>::success("stage");
    CHECK(ok.ok());
    CHECK(static_cast<bool>(ok));
    CHECK(ok.value() == "stage");
    CHECK(ok.take() == "stage");

    Result<std::string> bad =
        fail<std::string>(DiagnosticCode::asset_not_found, Subsystem::openusd, "gone");
    CHECK(!bad.ok());
    CHECK(!static_cast<bool>(bad));
    CHECK(bad.error().code() == DiagnosticCode::asset_not_found);
    CHECK(std::string(bad.error().id()) == "UGEO-E030");

    // Reading the wrong side of a result is a programming error, not a runtime
    // failure, and must not silently return a default-constructed value.
    bool threw = false;
    try {
        (void)bad.value();
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);

    threw = false;
    try {
        (void)ok.error();
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}

bool drop_named_member(const std::vector<std::string>& path, const std::string& name) {
    (void)path;
    return name != "drop";
}

bool keep_only_wanted_in_list(const std::vector<std::string>& path, const std::string& name) {
    if (path.size() == 2 && path[0] == "outer" && path[1] == "list") {
        return name == "wanted";
    }
    return true;
}

void test_json_reader() {
    json::Value document;
    std::string error;

    // A number is scanned by the JSON grammar, not by "advance over anything
    // numeric-looking", so a malformed one is rejected rather than kept.
    CHECK(!json::parse("{\"a\": 1-2e+3}", document, error));
    CHECK(!json::parse("{\"a\": 01}", document, error));
    CHECK(!json::parse("{\"a\": 1.}", document, error));
    CHECK(!json::parse("{\"a\": .5}", document, error));
    CHECK(json::parse("{\"a\": -1.5e-3}", document, error));
    CHECK(document.find("a") != nullptr && document.find("a")->is_number());

    // A dropped member is absent, and its siblings are unaffected. The dropped
    // value contains an escaped quote, which a skipper that stopped at the
    // first quote byte would misread as the end of the string.
    CHECK(json::parse("{\"drop\": \"a\\\"b\", \"keep\": \"yes\"}", document, error,
                      drop_named_member));
    CHECK(document.find("drop") == nullptr);
    CHECK(document.member_text("keep") == "yes");

    // Skipping is not the same as not looking: a malformed value still fails
    // the document, so a filter cannot hide a broken file.
    CHECK(!json::parse("{\"drop\": [1, }, \"keep\": 1}", document, error, drop_named_member));
    CHECK(!json::parse("{\"drop\": \"unterminated}", document, error, drop_named_member));

    // The filter sees the enclosing member names, so it can keep one field of
    // the elements of one array without touching identically named fields
    // elsewhere. That is what lets runtime_info read only `destination` out of
    // a composition lock's install list.
    CHECK(json::parse("{\"outer\": {\"list\": [{\"wanted\": \"a\", \"bulk\": \"b\"}],"
                      " \"other\": {\"bulk\": \"c\"}}}",
                      document, error, keep_only_wanted_in_list));
    const json::Value* list = document.find("outer")->find("list");
    CHECK(list != nullptr && list->items().size() == 1);
    CHECK(list->items()[0].member_text("wanted") == "a");
    CHECK(list->items()[0].find("bulk") == nullptr);
    CHECK(document.find("outer")->find("other")->member_text("bulk") == "c");
}

void test_runtime_info_document() {
    Result<RuntimeInfo> result =
        RuntimeInfo::from_lock_json(read("composition.lock.json"), "/example/prefix");
    if (!result.ok()) {
        std::fprintf(stderr, "FAILED: %s\n", result.error().to_json().c_str());
        std::abort();
    }
    const RuntimeInfo& info = result.value();

    CHECK(info.name() == "example-runtime");
    CHECK(info.prefix() == "/example/prefix");
    CHECK(info.target().id == "linux-x86_64-gcc13-py313");
    CHECK(info.target().os == "linux");
    CHECK(info.target().arch == "x86_64");
    CHECK(info.target().toolchain == "gcc13");
    CHECK(info.target().host_abi == "py313");
    CHECK(info.target().host_python_version == "3.13");
    CHECK(!info.target().host_python_bundled);
    CHECK(info.target().usd_version == "26.08");
    CHECK(info.identity().runtime_digest ==
          "sha256:5555555555555555555555555555555555555555555555555555555555555555");

    // Sorted, so two materializations of one runtime serialize identically.
    CHECK(info.components().size() == 4);
    CHECK(info.components()[0].id == "example-asset-io");
    CHECK(info.components()[3].id == "example-vector-plugins");
    CHECK(info.capabilities().size() == 4);
    CHECK(info.capabilities()[0].name == "library:example-asset-io");
    CHECK(info.capabilities()[3].name == "usd-fileformat:geojson");

    CHECK(info.component("example-usd") != nullptr);
    CHECK(info.component("example-usd")->kind == "runtime");
    CHECK(info.component("absent") == nullptr);
    CHECK(info.has_capability("usd-fileformat:geojson"));
    CHECK(!info.has_capability("usd-fileformat:tif"));
    CHECK(info.capability("usd-fileformat:geojson")->component == "example-vector-plugins");

    // The committed document is the shared contract: schemas/runtime-info.v1.json
    // validates it in the Python tooling tests, and this asserts that the SDK
    // is what produces it.
    CHECK(info.to_json() == read("runtime-info.json"));
}

void test_bundled_python_is_read_not_asserted() {
    Result<RuntimeInfo> result =
        RuntimeInfo::from_lock_json(read("bundled-python.lock.json"), "/example/prefix");
    CHECK(result.ok());
    CHECK(result.value().target().host_python_bundled);
    CHECK(result.value().target().os == "windows");
    CHECK(result.value().target().toolchain == "msvc143");
}

void test_runtime_info_rejects_bad_input() {
    const std::string good = read("composition.lock.json");

    Result<RuntimeInfo> broken = RuntimeInfo::from_lock_json("{\"schema\":", "/p");
    CHECK(!broken.ok());
    CHECK(broken.error().code() == DiagnosticCode::runtime_metadata_unreadable);

    Result<RuntimeInfo> future = RuntimeInfo::from_lock_json(
        "{\"schema\": \"openstrata.runtime-composition-lock/v9\"}", "/p");
    CHECK(!future.ok());
    CHECK(future.error().code() == DiagnosticCode::runtime_schema_unsupported);
    CHECK(future.error().detail("schema") == "openstrata.runtime-composition-lock/v9");

    // An ambiguous target must be refused rather than reported as empty fields.
    std::string ambiguous = good;
    const std::size_t at = ambiguous.find("linux-x86_64-gcc13-py313");
    CHECK(at != std::string::npos);
    ambiguous.replace(at, std::string("linux-x86_64-gcc13-py313").size(), "linux-x86_64-py313___");
    Result<RuntimeInfo> bad_target = RuntimeInfo::from_lock_json(ambiguous, "/p");
    CHECK(!bad_target.ok());
    CHECK(bad_target.error().code() == DiagnosticCode::runtime_metadata_invalid);

    // A composition with no OpenUSD is not a runtime this SDK can describe.
    std::string no_usd = good;
    const std::size_t usd = no_usd.find("\"capability\": \"usd\"");
    CHECK(usd != std::string::npos);
    no_usd.replace(usd, std::string("\"capability\": \"usd\"").size(), "\"capability\": \"nope\"");
    Result<RuntimeInfo> missing_usd = RuntimeInfo::from_lock_json(no_usd, "/p");
    CHECK(!missing_usd.ok());
    CHECK(missing_usd.error().code() == DiagnosticCode::runtime_metadata_invalid);

    // to_json promises a document that satisfies schemas/runtime-info.v1.json,
    // so anything that could only serialize into an invalid one is refused
    // here. Each case below would otherwise have become an empty string or an
    // unknown enum value in the output.
    struct Case {
        const char* find;
        const char* replace;
        const char* what;
    };
    const Case rejected[] = {
        {"\"runtime_digest\": \"sha256:5555555555555555555555555555555555555555555555555555555555555555\"",
         "\"runtime_digest\": \"sha256:abc\"", "a runtime digest that is not a sha256 digest"},
        {"\"manifest_digest\": \"sha256:1111111111111111111111111111111111111111111111111111111111111111\"",
         "\"manifest_digest\": \"\"", "an empty manifest digest"},
        {"\"kind\": \"library\"", "\"kind\": \"bundle\"", "a component of an unknown kind"},
        {"\"version\": \"0.4.2\"", "\"version\": \"\"", "a component or capability with no version"},
        {"\"digest\": \"sha256:6666666666666666666666666666666666666666666666666666666666666666\"",
         "\"digest\": \"SHA256:6666666666666666666666666666666666666666666666666666666666666666\"",
         "a digest in a spelling the schema does not accept"},
        {"gcc13", "13gcc", "a toolchain that does not start with a letter"},
    };
    for (const Case& item : rejected) {
        std::string mutated = good;
        const std::size_t found = mutated.find(item.find);
        Check(found != std::string::npos, item.what);
        mutated.replace(found, std::string(item.find).size(), item.replace);
        Result<RuntimeInfo> result = RuntimeInfo::from_lock_json(mutated, "/p");
        Check(!result.ok() && result.error().code() == DiagnosticCode::runtime_metadata_invalid,
              item.what);
    }
}

/// The two answers to "which formats does this runtime have" are joined, not
/// collapsed. A composed format OpenUSD produced no format for stays visible
/// as `not_loaded` and a dispatched format the composition never asked for
/// stays visible as `undeclared`, because reporting either list alone -- or
/// their intersection -- erases exactly the case a caller needs to act on.
void test_format_support() {
    Result<RuntimeInfo> runtime =
        RuntimeInfo::from_lock_json(read("composition.lock.json"), "/example/prefix");
    CHECK(runtime.ok());

    // OpenUSD's own formats plus one of the fixture's two composed plugins:
    // this is a runtime whose vector plugin is installed and did not load.
    const std::vector<std::string> registered = {"copc", "usda", "usdc"};
    const std::vector<Format> formats = format_support(runtime.value(), registered);
    CHECK(formats.size() == 4);

    CHECK(formats[0].extension == "copc");
    CHECK(formats[0].state == FormatState::available);
    CHECK(formats[0].usable());
    CHECK(formats[0].component == "example-pointcloud-plugins");

    CHECK(formats[1].extension == "geojson");
    CHECK(formats[1].state == FormatState::not_loaded);
    CHECK(formats[1].composed() && !formats[1].registered());
    // A format that is composed and absent names what to look at, which is the
    // whole reason the state is reported instead of the format being dropped.
    CHECK(formats[1].capability == "usd-fileformat:geojson");
    CHECK(formats[1].component == "example-vector-plugins");
    CHECK(formats[1].artifact.rfind("sha256:", 0) == 0);

    CHECK(formats[3].extension == "usdc");
    CHECK(formats[3].state == FormatState::undeclared);
    CHECK(!formats[3].composed() && formats[3].registered());
    CHECK(formats[3].component.empty());

    // The committed document is the shared contract, the same way
    // runtime-info.json is: schemas/formats.v1.json validates it in the Python
    // tooling tests and this asserts that the SDK is what produces it.
    CHECK(formats_to_json(formats) == read("formats.json"));
    CHECK(std::string(formats_schema()) == "usd-geospatial-runtime.formats/v1");

    // Extensions are compared, not displayed, so they are normalized once: a
    // caller's ".USDA" is OpenUSD's "usda" and must not become a second entry,
    // and a repeat must not promote an undeclared format to a composed one.
    const std::vector<Format> noisy =
        format_support(runtime.value(), {"usda", ".USDA", "", "COPC"});
    CHECK(noisy.size() == 3);
    CHECK(noisy[0].extension == "copc" && noisy[0].usable());
    CHECK(noisy[2].extension == "usda");
    CHECK(noisy[2].state == FormatState::undeclared);

    // A runtime that dispatches nothing still reports every format the
    // composition declared. That is the difference between "this runtime does
    // not read GeoJSON" and "this runtime is broken".
    const std::vector<Format> nothing = format_support(runtime.value(), {});
    CHECK(nothing.size() == 2);
    for (const Format& format : nothing) {
        CHECK(format.state == FormatState::not_loaded);
    }

    // A lock that spells a capability differently -- a capital letter here --
    // still resolves to one format, and the entry names the capability its own
    // extension implies. An entry that kept the lock's literal string would
    // contradict its own `extension` and fail schemas/formats.v1.json.
    std::string mutated = read("composition.lock.json");
    const std::string spelling = "usd-fileformat:geojson";
    const std::size_t at = mutated.find(spelling);
    CHECK(at != std::string::npos);
    mutated.replace(at, spelling.size(), "usd-fileformat:GeoJSON");
    Result<RuntimeInfo> odd = RuntimeInfo::from_lock_json(mutated, "/example/prefix");
    CHECK(odd.ok());
    const std::vector<Format> spelled = format_support(odd.value(), {"geojson"});
    CHECK(spelled.size() == 2);
    CHECK(spelled[1].extension == "geojson");
    CHECK(spelled[1].capability == "usd-fileformat:geojson");
    CHECK(spelled[1].state == FormatState::available);
}


/// `open` reports the capability a composition would have to add and
/// `format_support` reads the capability a composition did add. Both spell the
/// name here, so the two cannot drift apart.
void test_format_capability_names() {
    CHECK(format_capability("copc") == "usd-fileformat:copc");
    CHECK(format_capability(".TIF") == "usd-fileformat:tif");
    CHECK(format_capability("").empty());

    CHECK(format_extension("usd-fileformat:geojson") == "geojson");
    CHECK(format_extension("usd-fileformat:").empty());
    // A composed runtime resolves capabilities that are not file formats, and
    // they are not reported as extensions of any kind.
    CHECK(format_extension("usd").empty());
    CHECK(format_extension("usd-resolver:http").empty());
    CHECK(format_extension("library:example-asset-io").empty());

    CHECK(std::string(format_state_name(FormatState::available)) == "available");
    CHECK(std::string(format_state_name(FormatState::not_loaded)) == "not_loaded");
    CHECK(std::string(format_state_name(FormatState::undeclared)) == "undeclared");
}

void test_runtime_info_from_prefix() {
    Result<RuntimeInfo> empty = runtime_info(std::string());
    CHECK(!empty.ok());
    CHECK(empty.error().code() == DiagnosticCode::invalid_argument);

    Result<RuntimeInfo> absent = runtime_info(g_data + "/not-a-prefix");
    CHECK(!absent.ok());
    CHECK(absent.error().code() == DiagnosticCode::runtime_not_found);
    CHECK(!absent.error().detail("expected").empty());

    // A directory that exists but holds no composition lock is the same
    // condition as one that does not exist: it is not a composed runtime.
    Result<RuntimeInfo> not_a_runtime = runtime_info(g_data);
    CHECK(!not_a_runtime.ok());
    CHECK(not_a_runtime.error().code() == DiagnosticCode::runtime_not_found);

    CHECK(std::string(runtime_prefix_variable()) == "USD_GEOSPATIAL_RUNTIME");
    CHECK(std::string(runtime_info_schema()) == "usd-geospatial-runtime.runtime-info/v1");
}

/// A prefix that looks composed but whose lock cannot be read is its own
/// condition, separate from "there is no runtime here".
///
/// The unreadable lock is a directory in the lock's place, because that is the
/// one unreadable file every platform can produce without permissions or a
/// privileged user. It is also why runtime_info() names the condition itself
/// rather than leaving it to the stream: opening a directory fails on some
/// platforms and succeeds and reads empty on others, and the caller must get
/// the same code either way.
void test_runtime_info_rejects_an_unreadable_lock() {
    namespace fs = std::filesystem;
    std::error_code status;
    // The path is unique per process. Two runs of this suite at once -- a
    // multi-config build, `ctest -j`, two CI jobs on one runner -- would
    // otherwise delete each other's fixture and fail on the wrong condition.
    const std::string unique =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
        std::to_string(reinterpret_cast<std::uintptr_t>(&status));
    const fs::path prefix =
        fs::temp_directory_path(status) / ("usdgeospatial-unreadable-lock-" + unique);
    CHECK(!status);
    fs::create_directories(prefix / "metadata" / "composition.lock.json", status);
    CHECK(!status);

    Result<RuntimeInfo> result = runtime_info(prefix.string());
    CHECK(!result.ok());
    CHECK(result.error().code() == DiagnosticCode::runtime_metadata_unreadable);
    CHECK(!result.error().detail("path").empty());

    fs::remove_all(prefix, status);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <test data directory>\n", argv[0]);
        return 2;
    }
    g_data = argv[1];

    test_diagnostic_codes();
    test_diagnostic_details();
    test_result();
    test_json_reader();
    test_runtime_info_document();
    test_bundled_python_is_read_not_asserted();
    test_runtime_info_rejects_bad_input();
    test_format_support();
    test_format_capability_names();
    test_runtime_info_from_prefix();
    test_runtime_info_rejects_an_unreadable_lock();

    std::printf("usdGeospatialCore: %d checks passed\n", g_checks);
    return 0;
}
