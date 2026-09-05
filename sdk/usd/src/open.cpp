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

/// Take the text OpenUSD posted since the mark, as one message, and clear it.
///
/// A stage that fails to open usually posts several errors, and only the set of
/// them explains the failure. They are relayed as message text and never
/// parsed: the code is the contract, this is the detail a human needs.
///
/// Clearing is not optional. `open` returns a `Result`, so anything left on the
/// error list would surface later as OpenUSD noise on stderr or, worse, would
/// trip an outer `TfErrorMark` in caller code that had nothing to do with it.
std::string take_errors(pxr::TfErrorMark& mark) {
    std::string text;
    for (auto it = mark.GetBegin(); it != mark.GetEnd(); ++it) {
        if (!text.empty()) {
            text += "; ";
        }
        text += it->GetCommentary();
    }
    mark.Clear();
    return text;
}

/// Attach relayed OpenUSD text to a diagnostic the SDK decided on itself.
///
/// The condition stays the SDK's -- a missing file format is
/// `capability_unavailable` whether or not OpenUSD had something to say -- but
/// when a plugin failed to load, what it said is the only thing that explains
/// why the format is missing, so it is carried rather than discarded.
Diagnostic& attach(Diagnostic& diagnostic, const std::string& relayed) {
    if (!relayed.empty()) {
        diagnostic.with("openusd", relayed);
    }
    return diagnostic;
}

}  // namespace

Result<pxr::UsdStageRefPtr> open(const std::string& uri) {
    if (uri.empty()) {
        return fail<pxr::UsdStageRefPtr>(DiagnosticCode::invalid_argument, Subsystem::sdk,
                                         "an asset was requested with an empty URI");
    }

    // The mark is taken before the first call into OpenUSD, not before the
    // last. Resolver and plugin registration happen inside the calls below and
    // post their own errors; a mark that starts later neither reports them nor
    // clears them, which is exactly the case -- a plugin that failed to load --
    // where the caller most needs to be told.
    pxr::TfErrorMark mark;

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
            Diagnostic diagnostic(DiagnosticCode::unsupported_uri_scheme, Subsystem::sdk,
                                  "no resolver in this composition handles the URI scheme");
            diagnostic.with("uri", uri)
                .with("scheme", scheme)
                .with("registered", join_schemes(registered));
            return Result<pxr::UsdStageRefPtr>::failure(attach(diagnostic, take_errors(mark)));
        }
    }

    pxr::ArResolver& resolver = pxr::ArGetResolver();
    const std::string extension = resolver.GetExtension(uri);
    if (extension.empty()) {
        Diagnostic diagnostic(DiagnosticCode::invalid_argument, Subsystem::sdk,
                              "the URI carries no extension, so no file format can be selected");
        diagnostic.with("uri", uri);
        return Result<pxr::UsdStageRefPtr>::failure(attach(diagnostic, take_errors(mark)));
    }
    if (!pxr::SdfFileFormat::FindByExtension(extension)) {
        Diagnostic diagnostic(DiagnosticCode::capability_unavailable, Subsystem::sdk,
                              "this composition installs no file format for the extension");
        diagnostic.with("uri", uri)
            .with("extension", extension)
            .with("capability", "usd-fileformat:" + extension);
        return Result<pxr::UsdStageRefPtr>::failure(attach(diagnostic, take_errors(mark)));
    }

    const pxr::ArResolvedPath resolved = resolver.Resolve(uri);
    if (!resolved) {
        const std::string relayed = take_errors(mark);
        Diagnostic diagnostic(
            DiagnosticCode::asset_not_found,
            relayed.empty() ? Subsystem::sdk : Subsystem::openusd,
            relayed.empty() ? "the resolver did not resolve the URI to an asset" : relayed);
        return Result<pxr::UsdStageRefPtr>::failure(diagnostic.with("uri", uri));
    }

    const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(uri);
    if (!stage) {
        const std::string relayed = take_errors(mark);
        return Result<pxr::UsdStageRefPtr>::failure(
            Diagnostic(DiagnosticCode::stage_open_failed,
                       relayed.empty() ? Subsystem::sdk : Subsystem::openusd,
                       relayed.empty() ? "OpenUSD returned no stage and posted no error" : relayed)
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
