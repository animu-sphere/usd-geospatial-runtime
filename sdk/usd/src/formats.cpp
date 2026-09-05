// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/formats.h>

#include <pxr/base/tf/errorMark.h>
#include <pxr/usd/sdf/fileFormat.h>

#include <set>
#include <string>
#include <vector>

namespace usd_geospatial {

std::vector<std::string> registered_extensions() {
    // The mark is taken before the first call because plugin registration and
    // plugin loading both happen below and post their own errors; see the note
    // in the header on why they are cleared rather than kept.
    pxr::TfErrorMark mark;

    // FindAllFileFormatExtensions is the candidate list, not the answer. It
    // returns the extension index OpenUSD builds from plugInfo.json metadata
    // and never loads a plugin, so an extension appears there whether or not
    // the library behind it can be loaded at all. FindByExtension is what
    // loads it -- it is the same call `open` dispatches on, and it returns
    // null when the plugin fails to load -- so each candidate is asked.
    //
    // Answering costs the load of every registered format plugin. That is the
    // price of an answer that agrees with what `open` will do: a report built
    // from the metadata index alone would call a plugin that ships and cannot
    // load `available`, and `not_loaded` -- the state that exists to name
    // exactly that failure -- would be unreachable. `runtime_info` remains the
    // introspection that costs nothing and loads nothing.
    const std::set<std::string> candidates = pxr::SdfFileFormat::FindAllFileFormatExtensions();
    std::vector<std::string> dispatched;
    dispatched.reserve(candidates.size());
    for (const std::string& extension : candidates) {
        if (pxr::SdfFileFormat::FindByExtension(extension)) {
            dispatched.push_back(extension);
        }
    }

    mark.Clear();
    // The candidates were already sorted and unique; format_support normalizes
    // them anyway, so no assumption about OpenUSD's spelling is made here.
    return dispatched;
}

std::vector<Format> formats(const RuntimeInfo& runtime) {
    return format_support(runtime, registered_extensions());
}

Result<std::vector<Format>> formats() {
    Result<RuntimeInfo> runtime = runtime_info();
    if (!runtime) {
        return Result<std::vector<Format>>::failure(runtime.error());
    }
    return Result<std::vector<Format>>::success(formats(runtime.value()));
}

}  // namespace usd_geospatial
