// SPDX-License-Identifier: Apache-2.0
#ifndef USD_GEOSPATIAL_RUNTIME_INFO_H
#define USD_GEOSPATIAL_RUNTIME_INFO_H

#include <usd_geospatial/result.h>

#include <string>
#include <vector>

namespace usd_geospatial {

/// The canonical target, decomposed. `id` is the identity and every other
/// field is derived from it or from the resolved composition, exactly as
/// `tools/runtime_metadata.py` derives them for the committed metadata
/// document. A target string that cannot be decomposed is rejected rather than
/// reported with empty fields.
struct Target {
    std::string id;
    std::string os;
    std::string arch;
    std::string toolchain;
    std::string host_abi;
    std::string usd_version;
    std::string host_python_version;
    /// Whether the composition installs its own interpreter. Read from the
    /// resolved install list, not asserted.
    bool host_python_bundled = false;
};

/// The immutable identities of the composition this runtime was materialized
/// from. `runtime_digest` is the one a caller compares against a release
/// record or a lock to prove which runtime is loaded.
struct Identity {
    std::string manifest_digest;
    std::string composition_digest;
    std::string runtime_digest;
};

/// One resolved component and the OpenStrata artifact it came from.
struct Component {
    std::string id;
    std::string kind;
    std::string version;
    std::string artifact;
};

/// One capability and the component that provides it. This is the mapping a
/// caller consults to answer "can this runtime read a GeoTIFF" without opening
/// a file.
struct Capability {
    std::string name;
    std::string component;
    std::string version;
    std::string artifact;
};

/// Introspection over a materialized runtime prefix.
///
/// The values come from the prefix's own `metadata/composition.lock.json`, so
/// they describe the runtime that is actually present rather than the one a
/// build was configured against.
class RuntimeInfo {
public:
    /// The composition name, for example `usd-geospatial-runtime`.
    const std::string& name() const { return _name; }
    /// The prefix the values were read from. Not part of `to_json`: a runtime
    /// is identified by its digests, not by where it happened to be laid out.
    const std::string& prefix() const { return _prefix; }
    const Target& target() const { return _target; }
    const Identity& identity() const { return _identity; }
    const std::vector<Component>& components() const { return _components; }
    const std::vector<Capability>& capabilities() const { return _capabilities; }

    /// The component with an id, or nullptr. The pointer is owned by this
    /// object and is valid for as long as it is.
    const Component* component(const std::string& id) const;
    /// The provider of a capability, or nullptr.
    const Capability* capability(const std::string& name) const;
    bool has_capability(const std::string& name) const { return capability(name) != nullptr; }

    /// The runtime as a JSON object conforming to
    /// `schemas/runtime-info.v1.json`. Components and capabilities are sorted
    /// by id and by name, so equal runtimes serialize to equal text.
    std::string to_json() const;

    /// Populate from the text of a composition lock. Exposed so a caller can
    /// introspect a lock it already holds, and so the tests do not need a
    /// materialized prefix.
    static Result<RuntimeInfo> from_lock_json(const std::string& text, std::string prefix);

private:
    std::string _name;
    std::string _prefix;
    Target _target;
    Identity _identity;
    std::vector<Component> _components;
    std::vector<Capability> _capabilities;
};

/// The schema identifier `RuntimeInfo::to_json` writes.
const char* runtime_info_schema();

/// The environment variable the no-argument `runtime_info()` reads.
const char* runtime_prefix_variable();

/// Describe the runtime materialized at `prefix`.
Result<RuntimeInfo> runtime_info(const std::string& prefix);

/// Describe the runtime named by `USD_GEOSPATIAL_RUNTIME`. Fails with
/// `runtime_not_specified` when the variable is unset or empty, so an
/// unconfigured process gets a named condition rather than a path error.
Result<RuntimeInfo> runtime_info();

}  // namespace usd_geospatial

#endif  // USD_GEOSPATIAL_RUNTIME_INFO_H
