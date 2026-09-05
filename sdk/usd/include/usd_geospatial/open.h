// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_OPEN_H
#define USD_GEOSPATIAL_OPEN_H

#include <usd_geospatial/result.h>

#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>

#include <string>

namespace usd_geospatial {

/// Open an asset as an OpenUSD stage.
///
/// This is a convenience entry point, not a wrapper: the value on success is
/// the ordinary `UsdStageRefPtr` an OpenUSD caller already knows, and every
/// `pxr::*` API stays available below it. What the SDK adds is a typed failure
/// in place of the null pointer plus loose `TF_ERROR` text that OpenUSD returns
/// when an asset cannot be opened. The conditions it separates are:
///
/// - `invalid_argument` -- the URI is empty or carries no extension to
///   dispatch on, so nothing was attempted.
/// - `unsupported_uri_scheme` -- the URI names a scheme no resolver in this
///   composition is registered for. The `scheme` detail names it and the
///   `registered` detail lists the ones that would have worked.
/// - `capability_unavailable` -- the extension has no FileFormat plugin in
///   this composition. The `capability` detail carries the capability name
///   (`usd-fileformat:copc`, for example) that a composition would have to
///   provide, which is the same name the manifest and the runtime metadata
///   use, so a caller can report exactly what is missing.
/// - `asset_not_found` -- the resolver did not resolve the URI to anything.
/// - `stage_open_failed` -- the asset resolved and its format is present, but
///   OpenUSD declined to compose a stage. The relayed OpenUSD text is in the
///   message and the subsystem is `openusd`.
///
/// The first three are the SDK's own decisions, and each carries an `openusd`
/// detail when OpenUSD had also posted something while the call ran -- for a
/// missing capability that text is usually the reason a plugin failed to load,
/// which is the difference between a format that was never composed and one
/// that was composed and did not load. The last two relay that text as the
/// message instead, because there it is the whole explanation.
///
/// OpenUSD's error list is cleared on every path, so a caller's own
/// `TfErrorMark` never sees this call's diagnostics.
///
/// The formats this accepts are whatever the composition provides; the SDK
/// hardcodes no format list. `runtime_info().capabilities()` is the
/// machine-readable answer to what is available without opening anything.
Result<pxr::UsdStageRefPtr> open(const std::string& uri);

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_OPEN_H
