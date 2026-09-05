// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/formats.h>

#include <pxr/base/tf/errorMark.h>
#include <pxr/usd/sdf/fileFormat.h>

#include <set>
#include <string>
#include <vector>

namespace usd_geospatial {

std::vector<std::string> registered_extensions() {
    // The mark is taken before the call because this is where plugin
    // discovery happens for a process that has not opened anything yet; see
    // the note in the header on why the errors are cleared rather than kept.
    pxr::TfErrorMark mark;
    const std::set<std::string> found = pxr::SdfFileFormat::FindAllFileFormatExtensions();
    mark.Clear();
    // The set is already sorted and unique; format_support normalizes it
    // anyway, so no assumption about OpenUSD's spelling is made here.
    return std::vector<std::string>(found.begin(), found.end());
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
