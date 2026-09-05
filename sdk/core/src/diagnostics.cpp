// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/diagnostics.h>

#include "json.h"

#include <utility>

namespace usd_geospatial {
namespace {

const std::string kEmptyValue;

}  // namespace

// The three tables below are the published contract. A row's id and name are
// fixed once released; adding a condition appends a row, it never renumbers
// one. The switches are deliberately exhaustive with no `default`, so adding
// an enumerator without an id fails the build instead of shipping an empty
// code.

const char* diagnostic_code_id(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::invalid_argument: return "UGEO-E001";
        case DiagnosticCode::unsupported_uri_scheme: return "UGEO-E002";
        case DiagnosticCode::runtime_not_specified: return "UGEO-E010";
        case DiagnosticCode::runtime_not_found: return "UGEO-E011";
        case DiagnosticCode::runtime_metadata_unreadable: return "UGEO-E012";
        case DiagnosticCode::runtime_metadata_invalid: return "UGEO-E013";
        case DiagnosticCode::runtime_schema_unsupported: return "UGEO-E014";
        case DiagnosticCode::asset_not_found: return "UGEO-E030";
        case DiagnosticCode::capability_unavailable: return "UGEO-E031";
        case DiagnosticCode::stage_open_failed: return "UGEO-E032";
    }
    return "UGEO-E001";
}

const char* diagnostic_code_name(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::invalid_argument: return "invalid_argument";
        case DiagnosticCode::unsupported_uri_scheme: return "unsupported_uri_scheme";
        case DiagnosticCode::runtime_not_specified: return "runtime_not_specified";
        case DiagnosticCode::runtime_not_found: return "runtime_not_found";
        case DiagnosticCode::runtime_metadata_unreadable: return "runtime_metadata_unreadable";
        case DiagnosticCode::runtime_metadata_invalid: return "runtime_metadata_invalid";
        case DiagnosticCode::runtime_schema_unsupported: return "runtime_schema_unsupported";
        case DiagnosticCode::asset_not_found: return "asset_not_found";
        case DiagnosticCode::capability_unavailable: return "capability_unavailable";
        case DiagnosticCode::stage_open_failed: return "stage_open_failed";
    }
    return "invalid_argument";
}

Category diagnostic_category(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::invalid_argument:
        case DiagnosticCode::unsupported_uri_scheme:
            return Category::argument;
        case DiagnosticCode::runtime_not_specified:
        case DiagnosticCode::runtime_not_found:
        case DiagnosticCode::runtime_metadata_unreadable:
        case DiagnosticCode::runtime_metadata_invalid:
        case DiagnosticCode::runtime_schema_unsupported:
            return Category::runtime;
        case DiagnosticCode::asset_not_found:
        case DiagnosticCode::capability_unavailable:
        case DiagnosticCode::stage_open_failed:
            return Category::asset;
    }
    return Category::argument;
}

const char* category_name(Category category) {
    switch (category) {
        case Category::argument: return "argument";
        case Category::runtime: return "runtime";
        case Category::asset: return "asset";
    }
    return "argument";
}

const char* subsystem_name(Subsystem subsystem) {
    switch (subsystem) {
        case Subsystem::sdk: return "sdk";
        case Subsystem::openstrata: return "openstrata";
        case Subsystem::openusd: return "openusd";
    }
    return "sdk";
}

Diagnostic::Diagnostic(DiagnosticCode code, Subsystem subsystem, std::string message)
    : _code(code), _subsystem(subsystem), _message(std::move(message)) {}

Diagnostic& Diagnostic::with(std::string key, std::string value) {
    _details.push_back(Detail{std::move(key), std::move(value)});
    return *this;
}

const std::string& Diagnostic::detail(const std::string& key) const {
    for (const Detail& item : _details) {
        if (item.key == key) {
            return item.value;
        }
    }
    return kEmptyValue;
}

std::string Diagnostic::to_json() const {
    json::Writer writer;
    writer.begin_object();
    writer.key("code");
    writer.string(id());
    writer.key("name");
    writer.string(name());
    writer.key("category");
    writer.string(category_name(category()));
    writer.key("subsystem");
    writer.string(subsystem_name(_subsystem));
    writer.key("message");
    writer.string(_message);
    writer.key("details");
    writer.begin_object();
    for (const Detail& item : _details) {
        writer.key(item.key);
        writer.string(item.value);
    }
    writer.end_object();
    writer.end_object();
    return writer.text();
}

}  // namespace usd_geospatial
