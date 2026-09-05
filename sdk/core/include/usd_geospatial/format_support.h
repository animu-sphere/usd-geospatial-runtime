// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_FORMAT_SUPPORT_H
#define USD_GEOSPATIAL_FORMAT_SUPPORT_H

#include <usd_geospatial/runtime_info.h>

#include <string>
#include <vector>

namespace usd_geospatial {

/// What a runtime can do with one file extension.
///
/// Two sources answer "which formats does this runtime have", and they are not
/// the same question: the composition declares the capabilities it resolved,
/// and OpenUSD dispatches the file formats whose plugins actually loaded. They
/// agree in the ordinary case and disagree exactly when the answer matters
/// most, so neither is reported alone and neither is silently intersected --
/// each extension carries the state that says which of the two claimed it.
enum class FormatState {
    /// The composition resolves a `usd-fileformat:` capability for the
    /// extension and OpenUSD produced a file format for it. `open` will
    /// dispatch on it.
    available,
    /// The composition resolves the capability and OpenUSD produced no file
    /// format: the plugin never registered the extension, or it registered it
    /// and its library failed to load. Either way the composition installed
    /// something that does not work, which is what explains a
    /// `capability_unavailable` failure for a format the runtime metadata says
    /// is present, and the provider fields name what to look at.
    not_loaded,
    /// OpenUSD dispatches a file format the composition never asked for. This
    /// is the normal state of OpenUSD's own formats -- `usda`, `usdc`, `usdz`
    /// -- which no capability declares, and of anything else that reached the
    /// plugin path from outside the composition.
    undeclared,
};

/// One extension and what is known about it.
///
/// The provider fields are the composed side of the answer and are empty for
/// an `undeclared` format, because a composition that never declared the
/// extension has no component, version, or artifact to name for it.
struct Format {
    std::string extension;
    FormatState state = FormatState::undeclared;
    std::string capability;
    std::string component;
    std::string version;
    std::string artifact;

    /// The composition resolves a capability for this extension.
    bool composed() const { return state != FormatState::undeclared; }
    /// OpenUSD produced a file format for this extension.
    bool registered() const { return state != FormatState::not_loaded; }
    /// Both, which is the only state in which `open` can use the format.
    bool usable() const { return state == FormatState::available; }
};

/// The stable spelling of a state, matching the enumerator name.
const char* format_state_name(FormatState state);

/// The capability name an extension is composed under, for example
/// `usd-fileformat:copc`. This is the same name the composition manifest, the
/// runtime metadata, and the `capability` detail on `UGEO-E031` use, so a
/// consumer reporting a missing format names it the way a composition would
/// have to declare it.
std::string format_capability(const std::string& extension);

/// The extension a `usd-fileformat:` capability provides, lowercased, or an
/// empty string for any other capability. `usd` and `usd-resolver:http` are
/// capabilities of a composed runtime but they are not file formats.
std::string format_extension(const std::string& capability);

/// Join what a composition declares with what OpenUSD dispatches.
///
/// `registered` must be the extensions the loaded process will actually
/// dispatch, not the ones a plugInfo declares: a list built from metadata
/// alone reports a plugin that ships and cannot load as though it worked, and
/// makes `not_loaded` unreachable. `registered_extensions()` in the OpenUSD
/// lane obtains that list, and only that needs a loaded OpenUSD; taking it as
/// an argument is what keeps the rule that turns two lists into one answer
/// testable on a machine with no OpenUSD. Entries are lowercased,
/// deduplicated, and sorted by extension, so two reports of one runtime
/// compare as text.
std::vector<Format> format_support(const RuntimeInfo& runtime,
                                   const std::vector<std::string>& registered);

/// The schema identifier `formats_to_json` writes.
const char* formats_schema();

/// The report as a JSON object conforming to `schemas/formats.v1.json`.
std::string formats_to_json(const std::vector<Format>& formats);

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_FORMAT_SUPPORT_H
