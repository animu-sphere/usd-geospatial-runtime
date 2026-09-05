// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_DIAGNOSTICS_H
#define USD_GEOSPATIAL_DIAGNOSTICS_H

#include <string>
#include <vector>

/// The SDK spells its public surface in snake_case because the conceptual API
/// is shared across C++, Python, and JavaScript, and `open`, `inspect`,
/// `formats`, and `runtime_info` are named in docs/design/spec.md. Types stay
/// PascalCase so `runtime_info()` the function and `RuntimeInfo` the type do
/// not collide.
namespace usd_geospatial {

/// The stable failure vocabulary.
///
/// A code is an automation contract: once published, a name keeps its meaning
/// and its `UGEO-Ennn` id forever, and neither is reused. There is deliberately
/// no generic `unknown` -- every failure path names its condition, so a caller
/// can branch on the cause rather than parse the message. Message text is not
/// a contract and may change in any release.
///
/// Ids are assigned in blocks so a later code joins its own group without
/// renumbering: E001-E009 argument, E010-E029 runtime, E030-E049 asset.
enum class DiagnosticCode {
    /// UGEO-E001: an argument is empty or malformed before any I/O is tried.
    invalid_argument,
    /// UGEO-E002: the URI scheme is not one the composed resolvers handle.
    unsupported_uri_scheme,

    /// UGEO-E010: no runtime prefix was passed and the environment names none.
    runtime_not_specified,
    /// UGEO-E011: the named prefix is missing or is not a composed runtime.
    runtime_not_found,
    /// UGEO-E012: the runtime metadata could not be opened or is not JSON.
    runtime_metadata_unreadable,
    /// UGEO-E013: the runtime metadata parsed but does not describe a runtime.
    runtime_metadata_invalid,
    /// UGEO-E014: the runtime metadata declares a schema this SDK cannot read.
    runtime_schema_unsupported,

    /// UGEO-E030: the asset could not be resolved to something readable.
    asset_not_found,
    /// UGEO-E031: no composed component provides the capability the asset needs.
    capability_unavailable,
    /// UGEO-E032: the asset resolved and its format is present, but OpenUSD
    /// declined to open it as a stage.
    stage_open_failed,
};

/// The coarse grouping a code belongs to. Callers that only need to decide
/// between "the caller passed something wrong", "the runtime is not usable",
/// and "this asset is not usable" branch on this instead of on every code.
enum class Category {
    argument,
    runtime,
    asset,
};

/// Which layer observed the failure. This is not derivable from the code: the
/// same `stage_open_failed` may be reported by the SDK's own checks or relayed
/// from OpenUSD, and a caller triaging a bug needs to know which.
enum class Subsystem {
    sdk,
    openstrata,
    openusd,
};

/// One structured key/value fact about a failure, such as the capability that
/// was missing or the extension that was not recognized. Details carry the
/// machine-readable specifics that would otherwise only exist inside `message`.
struct Detail {
    std::string key;
    std::string value;
};

/// The stable `UGEO-Ennn` id of a code. Never localized; this is an identifier.
const char* diagnostic_code_id(DiagnosticCode code);

/// The stable spelling of a code, matching the enumerator name.
const char* diagnostic_code_name(DiagnosticCode code);

/// The category a code belongs to.
Category diagnostic_category(DiagnosticCode code);

const char* category_name(Category category);
const char* subsystem_name(Subsystem subsystem);

/// A typed failure.
///
/// Every accessor except `message` is part of the automation contract. A
/// binding is expected to preserve `id`, `name`, `category`, `subsystem`, and
/// `details` through whatever idiomatic error type its language uses, so a
/// Python or Node caller can branch on the same facts a C++ caller does.
class Diagnostic {
public:
    Diagnostic(DiagnosticCode code, Subsystem subsystem, std::string message);

    DiagnosticCode code() const { return _code; }
    const char* id() const { return diagnostic_code_id(_code); }
    const char* name() const { return diagnostic_code_name(_code); }
    Category category() const { return diagnostic_category(_code); }
    Subsystem subsystem() const { return _subsystem; }
    const std::string& message() const { return _message; }
    const std::vector<Detail>& details() const { return _details; }

    /// Attach one structured fact. Returns `*this` so a failure can be built
    /// and returned in a single expression.
    Diagnostic& with(std::string key, std::string value);

    /// The value recorded for a key, or an empty string when the key is absent.
    /// An absent key and an empty value are deliberately not distinguished:
    /// a detail with no value carries no information either way.
    const std::string& detail(const std::string& key) const;

    /// The diagnostic as a JSON object, for logs and for bindings that hand
    /// failures to tooling. Field names are stable; `message` is not.
    std::string to_json() const;

private:
    DiagnosticCode _code;
    Subsystem _subsystem;
    std::string _message;
    std::vector<Detail> _details;
};

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_DIAGNOSTICS_H
