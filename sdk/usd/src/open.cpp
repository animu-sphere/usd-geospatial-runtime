// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/open.h>

#include <pxr/base/tf/errorMark.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/fileFormat.h>

#include <string>
#include <vector>

namespace usd_geospatial {
namespace {

/// The scheme of a URI, or an empty string when the reference is a plain path.
///
/// Only `scheme://` counts. A Windows path such as `C:/data/site.copc` also
/// contains a colon, and treating its drive letter as a scheme would turn every
/// absolute Windows path into an unsupported-scheme failure.
std::string uri_scheme(const std::string& uri) {
    const std::size_t mark = uri.find("://");
    if (mark == std::string::npos || mark == 0) {
        return std::string();
    }
    std::string scheme = uri.substr(0, mark);
    for (const char c : scheme) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
        if (!ok) {
            return std::string();
        }
    }
    for (char& c : scheme) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return scheme;
}

std::string join_schemes(const std::vector<std::string>& schemes) {
    std::string joined;
    for (const std::string& scheme : schemes) {
        if (!joined.empty()) {
            joined += ",";
        }
        joined += scheme;
    }
    return joined;
}

/// The text OpenUSD posted while an operation ran, as one message.
///
/// A stage that fails to open usually posts several errors, and only the set of
/// them explains the failure. They are relayed as message text and never
/// parsed: `stage_open_failed` is the contract, this is the detail a human
/// needs.
std::string collect_errors(const pxr::TfErrorMark& mark) {
    std::string text;
    for (auto it = mark.GetBegin(); it != mark.GetEnd(); ++it) {
        if (!text.empty()) {
            text += "; ";
        }
        text += it->GetCommentary();
    }
    return text;
}

}  // namespace

Result<pxr::UsdStageRefPtr> open(const std::string& uri) {
    if (uri.empty()) {
        return fail<pxr::UsdStageRefPtr>(DiagnosticCode::invalid_argument, Subsystem::sdk,
                                         "an asset was requested with an empty URI");
    }

    const std::string scheme = uri_scheme(uri);
    if (!scheme.empty()) {
        const std::vector<std::string>& registered = pxr::ArGetRegisteredURISchemes();
        bool known = false;
        for (const std::string& candidate : registered) {
            if (candidate == scheme) {
                known = true;
                break;
            }
        }
        if (!known) {
            return Result<pxr::UsdStageRefPtr>::failure(
                Diagnostic(DiagnosticCode::unsupported_uri_scheme, Subsystem::sdk,
                           "no resolver in this composition handles the URI scheme")
                    .with("uri", uri)
                    .with("scheme", scheme)
                    .with("registered", join_schemes(registered)));
        }
    }

    pxr::ArResolver& resolver = pxr::ArGetResolver();
    const std::string extension = resolver.GetExtension(uri);
    if (extension.empty()) {
        return Result<pxr::UsdStageRefPtr>::failure(
            Diagnostic(DiagnosticCode::invalid_argument, Subsystem::sdk,
                       "the URI carries no extension, so no file format can be selected")
                .with("uri", uri));
    }
    if (!pxr::SdfFileFormat::FindByExtension(extension)) {
        return Result<pxr::UsdStageRefPtr>::failure(
            Diagnostic(DiagnosticCode::capability_unavailable, Subsystem::sdk,
                       "this composition installs no file format for the extension")
                .with("uri", uri)
                .with("extension", extension)
                .with("capability", "usd-fileformat:" + extension));
    }

    pxr::TfErrorMark mark;
    const pxr::ArResolvedPath resolved = resolver.Resolve(uri);
    if (!resolved) {
        Diagnostic diagnostic(DiagnosticCode::asset_not_found, Subsystem::openusd,
                              collect_errors(mark));
        mark.Clear();
        if (diagnostic.message().empty()) {
            diagnostic = Diagnostic(DiagnosticCode::asset_not_found, Subsystem::sdk,
                                    "the resolver did not resolve the URI to an asset");
        }
        return Result<pxr::UsdStageRefPtr>::failure(diagnostic.with("uri", uri));
    }

    const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(uri);
    if (!stage) {
        const std::string text = collect_errors(mark);
        mark.Clear();
        return Result<pxr::UsdStageRefPtr>::failure(
            Diagnostic(DiagnosticCode::stage_open_failed,
                       text.empty() ? Subsystem::sdk : Subsystem::openusd,
                       text.empty() ? "OpenUSD returned no stage and posted no error" : text)
                .with("uri", uri)
                .with("extension", extension));
    }

    // A stage can open while OpenUSD still posts recoverable errors. The stage
    // is the result the caller asked for, so those are dropped rather than
    // turned into a failure the caller cannot act on.
    mark.Clear();
    return Result<pxr::UsdStageRefPtr>::success(stage);
}

}  // namespace usd_geospatial
