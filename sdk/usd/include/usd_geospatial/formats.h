// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_FORMATS_H
#define USD_GEOSPATIAL_FORMATS_H

#include <usd_geospatial/format_support.h>
#include <usd_geospatial/result.h>
#include <usd_geospatial/runtime_info.h>

#include <string>
#include <vector>

namespace usd_geospatial {

/// The extensions OpenUSD has a file format for in this process, lowercased
/// and sorted.
///
/// OpenUSD posts its own errors while it registers plugins, and this clears
/// them for the same reason `open` does: the caller gets a value, so anything
/// left on the error list would surface later as unexplained noise or trip an
/// unrelated `TfErrorMark`. Nothing is lost that the report does not already
/// carry -- a plugin that failed to load is exactly what `not_loaded` says --
/// and the text itself is still relayed by `open` when a caller tries to use
/// the format.
std::vector<std::string> registered_extensions();

/// Report every file format this runtime knows about.
///
/// The report is the union of two lists that are usually the same and are not
/// the same question. `runtime_info().capabilities()` is what the composition
/// resolved: it is readable without OpenUSD and it is a claim about what was
/// installed. What OpenUSD registered is a claim about what loaded. A format
/// that is composed but not registered is a plugin that failed to load, and a
/// format that is registered but not composed is one this composition never
/// asked for -- OpenUSD's own `usda`, `usdc`, and `usdz`, or something that
/// reached the plugin path from outside. Reporting only one list, or only
/// their intersection, would erase the difference exactly where a caller needs
/// it, so every extension is reported with its `FormatState`.
///
/// These declarations are in the OpenUSD lane, and only these, because the
/// registered list can only be had from a loaded OpenUSD. The rule that turns
/// the two lists into one report is `format_support` in the core lane, where
/// it is tested without a runtime.
///
/// No signature here exposes a `pxr::` type: a binding can project `formats`
/// without projecting OpenUSD.
///
/// This overload takes a runtime that has already been described, so a prefix
/// is read once rather than twice.
std::vector<Format> formats(const RuntimeInfo& runtime);

/// The formats of the runtime named by `USD_GEOSPATIAL_RUNTIME`.
///
/// This fails with the same runtime diagnostics `runtime_info()` does, because
/// half the answer comes from the composition lock: a process that cannot say
/// which runtime it loaded cannot report what that runtime composed either.
Result<std::vector<Format>> formats();

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_FORMATS_H
