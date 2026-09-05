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
#include <usd_geospatial/result.h>
#include <usd_geospatial/runtime_info.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

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
    CHECK(info.components().size() == 3);
    CHECK(info.components()[0].id == "example-asset-io");
    CHECK(info.components()[2].id == "example-vector-plugins");
    CHECK(info.capabilities().size() == 3);
    CHECK(info.capabilities()[0].name == "library:example-asset-io");

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
    test_runtime_info_document();
    test_bundled_python_is_read_not_asserted();
    test_runtime_info_rejects_bad_input();
    test_runtime_info_from_prefix();

    std::printf("usdGeospatialCore: %d checks passed\n", g_checks);
    return 0;
}
