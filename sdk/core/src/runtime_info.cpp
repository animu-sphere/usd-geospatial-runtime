// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/runtime_info.h>

#include "json.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace usd_geospatial {
namespace {

/// The lock schema this SDK knows how to read. A prefix written by a future
/// OpenStrata that changes the shape is refused by name rather than
/// misinterpreted field by field.
constexpr const char* kLockSchema = "openstrata.runtime-composition-lock/v1alpha1";

constexpr const char* kRuntimeInfoSchema = "usd-geospatial-runtime.runtime-info/v1";
constexpr const char* kPrefixVariable = "USD_GEOSPATIAL_RUNTIME";

/// Where a materialized prefix keeps the lock it was composed from.
constexpr const char* kLockRelativePath = "metadata/composition.lock.json";

std::string join(const std::string& prefix, const char* relative) {
    std::string base = prefix;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\')) {
        base.pop_back();
    }
    return base + "/" + relative;
}

Diagnostic invalid(std::string message) {
    return Diagnostic(DiagnosticCode::runtime_metadata_invalid, Subsystem::openstrata,
                      std::move(message));
}

/// Whether a value is a `sha256:` digest, in the one spelling
/// `schemas/runtime-info.v1.json` accepts.
bool is_digest(const std::string& value) {
    static const std::string prefix = "sha256:";
    if (value.size() != prefix.size() + 64 || value.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    for (std::size_t at = prefix.size(); at < value.size(); ++at) {
        const char c = value[at];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

/// Which members of a composition lock are worth building.
///
/// A lock is mostly file inventory: of the 6.2 MB one this repository commits,
/// about 5 MB is `inventory`, `sdk`, `artifacts`, and `manifest`, none of which
/// this SDK reads, and another 1.1 MB is the install list, of which it reads
/// only the destination of each entry. `runtime_info` is documented as the
/// cheap probe a caller runs before OpenUSD loads, so the rest is scanned for
/// well-formedness and discarded instead of becoming a hundred thousand nodes.
bool keep_runtime_members(const std::vector<std::string>& path, const std::string& name) {
    if (path.empty()) {
        return name == "schema" || name == "resolved" || name == "runtime_digest";
    }
    if (path.size() == 2 && path[0] == "resolved" && path[1] == "install") {
        return name == "destination";
    }
    return true;
}

/// Decompose the canonical target string, which is the target's identity:
/// `<os>-<arch>-<toolchain>-py<host python>`. This is the same shape
/// `tools/runtime_metadata.py` enforces for the committed metadata document,
/// and it is enforced here for the same reason: a target that cannot be
/// decomposed would otherwise acquire an ambiguous identity made of empty
/// fields.
bool decompose_target(const std::string& id, Target& target, std::string& error) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t dash = id.find('-', start);
        if (dash == std::string::npos) {
            parts.push_back(id.substr(start));
            break;
        }
        parts.push_back(id.substr(start, dash - start));
        start = dash + 1;
    }
    if (parts.size() != 4) {
        error = "target '" + id + "' is not canonical; expected <os>-<arch>-<toolchain>-py<version>";
        return false;
    }

    const auto lower_alnum = [](const std::string& value, bool allow_underscore) {
        if (value.empty()) {
            return false;
        }
        for (const char c : value) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                            (allow_underscore && c == '_');
            if (!ok) {
                return false;
            }
        }
        return true;
    };
    if (!lower_alnum(parts[0], false) || !lower_alnum(parts[1], true) ||
        !lower_alnum(parts[2], false)) {
        error = "target '" + id + "' contains a field that is not lowercase alphanumeric";
        return false;
    }
    // schemas/runtime-info.v1.json spells the toolchain `^[a-z][a-z0-9]*$`, so
    // a leading digit is refused here rather than serialized into a document
    // that then fails its own schema.
    if (parts[2][0] < 'a' || parts[2][0] > 'z') {
        error = "target '" + id + "' has a toolchain that does not start with a letter";
        return false;
    }
    if (parts[0] != "windows" && parts[0] != "linux" && parts[0] != "macos") {
        error = "target '" + id + "' names an unknown operating system";
        return false;
    }

    const std::string& abi = parts[3];
    if (abi.rfind("py", 0) != 0) {
        error = "target '" + id + "' does not end in a py<version> host ABI";
        return false;
    }
    const std::string digits = abi.substr(2);
    if (digits.size() < 2 || digits.size() > 3) {
        error = "target '" + id + "' host ABI does not carry two or three version digits";
        return false;
    }
    for (const char c : digits) {
        if (c < '0' || c > '9') {
            error = "target '" + id + "' host ABI is not numeric";
            return false;
        }
    }

    target.id = id;
    target.os = parts[0];
    target.arch = parts[1];
    target.toolchain = parts[2];
    target.host_abi = abi;
    target.host_python_version = digits.substr(0, 1) + "." + digits.substr(1);
    return true;
}

/// Whether an installed path is a Python interpreter, which is how the
/// composition answers "is an interpreter bundled" without being told. This
/// mirrors the INTERPRETER_PATTERN in `tools/runtime_metadata.py`.
bool is_interpreter(const std::string& destination) {
    const std::size_t slash = destination.find_last_of('/');
    std::string base = slash == std::string::npos ? destination : destination.substr(slash + 1);
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (base.rfind("python", 0) != 0) {
        return false;
    }
    std::size_t at = 6;
    while (at < base.size() && ((base[at] >= '0' && base[at] <= '9') || base[at] == '.')) {
        ++at;
    }
    const std::string rest = base.substr(at);
    if (rest.empty() || rest == ".exe") {
        return true;
    }
    // The version scan above consumes a trailing dot that belongs to the
    // extension, so `python3.13.exe` arrives here as a bare `exe`.
    return rest == "exe" && at > 6 && base[at - 1] == '.';
}

}  // namespace

const char* runtime_info_schema() { return kRuntimeInfoSchema; }

const char* runtime_prefix_variable() { return kPrefixVariable; }

const Component* RuntimeInfo::component(const std::string& id) const {
    for (const Component& item : _components) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

const Capability* RuntimeInfo::capability(const std::string& name) const {
    for (const Capability& item : _capabilities) {
        if (item.name == name) {
            return &item;
        }
    }
    return nullptr;
}

Result<RuntimeInfo> RuntimeInfo::from_lock_json(const std::string& text, std::string prefix) {
    json::Value document;
    std::string error;
    if (!json::parse(text, document, error, keep_runtime_members)) {
        return fail<RuntimeInfo>(DiagnosticCode::runtime_metadata_unreadable,
                                 Subsystem::openstrata,
                                 "the composition lock is not valid JSON: " + error);
    }
    if (!document.is_object()) {
        return Result<RuntimeInfo>::failure(invalid("the composition lock is not a JSON object"));
    }

    const std::string& schema = document.member_text("schema");
    if (schema != kLockSchema) {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_schema_unsupported, Subsystem::openstrata,
                       "the composition lock declares a schema this SDK cannot read")
                .with("schema", schema)
                .with("supported", kLockSchema));
    }

    const json::Value* resolved = document.find("resolved");
    if (resolved == nullptr || !resolved->is_object()) {
        return Result<RuntimeInfo>::failure(invalid("the composition lock has no resolved section"));
    }

    RuntimeInfo info;
    info._prefix = std::move(prefix);
    info._name = resolved->member_text("name");
    if (info._name.empty()) {
        return Result<RuntimeInfo>::failure(invalid("the composition lock names no composition"));
    }

    std::string target_error;
    if (!decompose_target(resolved->member_text("target"), info._target, target_error)) {
        return Result<RuntimeInfo>::failure(invalid(target_error));
    }

    // Every field below is checked, not just read. `to_json` promises a
    // document that conforms to schemas/runtime-info.v1.json, and the only way
    // to keep that promise is to refuse a lock that cannot produce one: an
    // absent digest would otherwise be serialized as "" and fail the schema at
    // whoever consumed the output rather than here, where the cause is known.
    info._identity.manifest_digest = resolved->member_text("manifest_digest");
    info._identity.composition_digest = resolved->member_text("composition_digest");
    info._identity.runtime_digest = document.member_text("runtime_digest");
    if (!is_digest(info._identity.manifest_digest) ||
        !is_digest(info._identity.composition_digest) ||
        !is_digest(info._identity.runtime_digest)) {
        return Result<RuntimeInfo>::failure(
            invalid("the composition lock does not record all three identities as sha256 digests"));
    }

    const json::Value* components = resolved->find("components");
    if (components == nullptr || !components->is_array() || components->items().empty()) {
        return Result<RuntimeInfo>::failure(invalid("the composition lock resolves no components"));
    }
    for (const json::Value& entry : components->items()) {
        Component component;
        component.id = entry.member_text("id");
        component.kind = entry.member_text("kind");
        component.version = entry.member_text("version");
        component.artifact = entry.member_text("digest");
        if (component.id.empty() || component.version.empty()) {
            return Result<RuntimeInfo>::failure(
                invalid("a resolved component has no id or no version"));
        }
        if (component.kind != "runtime" && component.kind != "library" &&
            component.kind != "plugin") {
            return Result<RuntimeInfo>::failure(
                invalid("resolved component '" + component.id + "' has an unknown kind"));
        }
        if (!is_digest(component.artifact)) {
            return Result<RuntimeInfo>::failure(
                invalid("resolved component '" + component.id + "' has no artifact digest"));
        }
        info._components.push_back(std::move(component));
    }

    const json::Value* providers = resolved->find("providers");
    if (providers == nullptr || !providers->is_array() || providers->items().empty()) {
        return Result<RuntimeInfo>::failure(invalid("the composition lock resolves no capabilities"));
    }
    for (const json::Value& entry : providers->items()) {
        Capability capability;
        capability.name = entry.member_text("capability");
        capability.component = entry.member_text("component");
        capability.version = entry.member_text("version");
        capability.artifact = entry.member_text("digest");
        if (capability.name.empty() || capability.component.empty() ||
            capability.version.empty()) {
            return Result<RuntimeInfo>::failure(
                invalid("a resolved capability has no name, no provider, or no version"));
        }
        if (!is_digest(capability.artifact)) {
            return Result<RuntimeInfo>::failure(
                invalid("capability '" + capability.name + "' has no artifact digest"));
        }
        info._capabilities.push_back(std::move(capability));
    }

    // Sorting is what makes two materializations of the same runtime serialize
    // to identical text, which is what lets a caller compare documents rather
    // than fields.
    std::sort(info._components.begin(), info._components.end(),
              [](const Component& left, const Component& right) { return left.id < right.id; });
    std::sort(info._capabilities.begin(), info._capabilities.end(),
              [](const Capability& left, const Capability& right) { return left.name < right.name; });

    const Capability* usd = info.capability("usd");
    if (usd == nullptr) {
        return Result<RuntimeInfo>::failure(
            invalid("the composition does not resolve the 'usd' capability"));
    }
    info._target.usd_version = usd->version;

    const json::Value* install = resolved->find("install");
    if (install != nullptr && install->is_array()) {
        for (const json::Value& entry : install->items()) {
            if (is_interpreter(entry.member_text("destination"))) {
                info._target.host_python_bundled = true;
                break;
            }
        }
    }

    return Result<RuntimeInfo>::success(std::move(info));
}

std::string RuntimeInfo::to_json() const {
    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.string(kRuntimeInfoSchema);

    writer.key("runtime");
    writer.begin_object();
    writer.key("name");
    writer.string(_name);
    writer.end_object();

    writer.key("target");
    writer.begin_object();
    writer.key("id");
    writer.string(_target.id);
    writer.key("os");
    writer.string(_target.os);
    writer.key("arch");
    writer.string(_target.arch);
    writer.key("toolchain");
    writer.string(_target.toolchain);
    writer.key("host_abi");
    writer.string(_target.host_abi);
    writer.key("usd_version");
    writer.string(_target.usd_version);
    writer.key("host_python");
    writer.begin_object();
    writer.key("version");
    writer.string(_target.host_python_version);
    writer.key("bundled");
    writer.boolean(_target.host_python_bundled);
    writer.end_object();
    writer.end_object();

    writer.key("identity");
    writer.begin_object();
    writer.key("manifest_digest");
    writer.string(_identity.manifest_digest);
    writer.key("composition_digest");
    writer.string(_identity.composition_digest);
    writer.key("runtime_digest");
    writer.string(_identity.runtime_digest);
    writer.end_object();

    writer.key("components");
    writer.begin_array();
    for (const Component& component : _components) {
        writer.begin_object();
        writer.key("id");
        writer.string(component.id);
        writer.key("kind");
        writer.string(component.kind);
        writer.key("version");
        writer.string(component.version);
        writer.key("artifact");
        writer.string(component.artifact);
        writer.end_object();
    }
    writer.end_array();

    writer.key("capabilities");
    writer.begin_array();
    for (const Capability& capability : _capabilities) {
        writer.begin_object();
        writer.key("capability");
        writer.string(capability.name);
        writer.key("component");
        writer.string(capability.component);
        writer.key("version");
        writer.string(capability.version);
        writer.key("artifact");
        writer.string(capability.artifact);
        writer.end_object();
    }
    writer.end_array();

    writer.end_object();
    return writer.text();
}

Result<RuntimeInfo> runtime_info(const std::string& prefix) {
    if (prefix.empty()) {
        return fail<RuntimeInfo>(DiagnosticCode::invalid_argument, Subsystem::sdk,
                                 "a runtime prefix was requested with an empty path");
    }
    const std::string lock_path = join(prefix, kLockRelativePath);

    std::error_code status;
    if (!std::filesystem::exists(lock_path, status)) {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_not_found, Subsystem::sdk,
                       "the path is not a composed runtime prefix")
                .with("prefix", prefix)
                .with("expected", lock_path));
    }

    // A path that exists but is not a regular file -- a directory named like
    // the lock, or a dangling symlink -- is refused here rather than left to
    // the stream, because whether opening a directory fails, or succeeds and
    // reads as empty, differs by platform and standard library. The condition
    // is the same one either outcome would eventually report; naming it here
    // makes it the same on every host.
    if (!std::filesystem::is_regular_file(lock_path, status)) {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_metadata_unreadable, Subsystem::sdk,
                       "the composition lock is not a regular file")
                .with("path", lock_path));
    }

    std::ifstream file(lock_path, std::ios::binary);
    if (!file) {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_metadata_unreadable, Subsystem::sdk,
                       "the composition lock exists but could not be opened")
                .with("path", lock_path));
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_metadata_unreadable, Subsystem::sdk,
                       "the composition lock could not be read to the end")
                .with("path", lock_path));
    }
    return RuntimeInfo::from_lock_json(buffer.str(), prefix);
}

Result<RuntimeInfo> runtime_info() {
    const char* prefix = std::getenv(kPrefixVariable);
    if (prefix == nullptr || *prefix == '\0') {
        return Result<RuntimeInfo>::failure(
            Diagnostic(DiagnosticCode::runtime_not_specified, Subsystem::sdk,
                       "no runtime prefix was given and the environment names none")
                .with("variable", kPrefixVariable));
    }
    return runtime_info(std::string(prefix));
}

}  // namespace usd_geospatial
