// SPDX-License-Identifier: Apache-2.0
#include <usd_geospatial/format_support.h>

#include "json.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace usd_geospatial {
namespace {

/// The capability namespace a file format is composed under. One capability
/// names one extension: the manifest declares `usd-fileformat:las` and
/// `usd-fileformat:laz` separately even though one component provides both.
constexpr const char* kFormatCapabilityPrefix = "usd-fileformat:";

constexpr const char* kFormatsSchema = "usd-geospatial-runtime.formats/v1";

/// Extensions are compared, not displayed, so they are normalized once here
/// rather than at every comparison. A leading dot is tolerated because a
/// caller building the registered list by hand is likely to write one; OpenUSD
/// itself reports extensions without it.
std::string normalize_extension(const std::string& value) {
    std::size_t at = 0;
    while (at < value.size() && value[at] == '.') {
        ++at;
    }
    std::string extension = value.substr(at);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

Format* find(std::vector<Format>& formats, const std::string& extension) {
    for (Format& format : formats) {
        if (format.extension == extension) {
            return &format;
        }
    }
    return nullptr;
}

}  // namespace

const char* format_state_name(FormatState state) {
    // Exhaustive with no default, so a state added later cannot serialize as
    // an empty string; the same rule the diagnostic tables follow.
    switch (state) {
        case FormatState::available: return "available";
        case FormatState::not_loaded: return "not_loaded";
        case FormatState::undeclared: return "undeclared";
    }
    return "undeclared";
}

std::string format_capability(const std::string& extension) {
    const std::string normalized = normalize_extension(extension);
    if (normalized.empty()) {
        return std::string();
    }
    return std::string(kFormatCapabilityPrefix) + normalized;
}

std::string format_extension(const std::string& capability) {
    const std::string prefix(kFormatCapabilityPrefix);
    if (capability.size() <= prefix.size() || capability.compare(0, prefix.size(), prefix) != 0) {
        return std::string();
    }
    return normalize_extension(capability.substr(prefix.size()));
}

std::vector<Format> format_support(const RuntimeInfo& runtime,
                                   const std::vector<std::string>& registered) {
    std::vector<Format> formats;

    // The composed side first, so every declared format appears even when
    // OpenUSD dispatches nothing at all -- which is what a caller inspecting a
    // runtime whose plugins failed to load needs to see.
    for (const Capability& capability : runtime.capabilities()) {
        const std::string extension = format_extension(capability.name);
        if (extension.empty() || find(formats, extension) != nullptr) {
            continue;
        }
        Format format;
        format.extension = extension;
        format.state = FormatState::not_loaded;
        // The canonical spelling of the extension, not the lock's literal
        // string. The two are the same in every conforming lock, and where a
        // lock spells a capability differently -- a capital letter, a leading
        // dot -- an entry that kept it would name a capability its own
        // `extension` contradicts and would fail schemas/formats.v1.json.
        format.capability = format_capability(extension);
        format.component = capability.component;
        format.version = capability.version;
        format.artifact = capability.artifact;
        formats.push_back(std::move(format));
    }

    for (const std::string& entry : registered) {
        const std::string extension = normalize_extension(entry);
        if (extension.empty()) {
            continue;
        }
        Format* known = find(formats, extension);
        if (known != nullptr) {
            // Only a declared format is promoted. A repeated registered
            // extension -- OpenUSD reports none, but a caller assembling the
            // list by hand can -- must not turn an undeclared format into a
            // composed one.
            if (known->composed()) {
                known->state = FormatState::available;
            }
            continue;
        }
        Format format;
        format.extension = extension;
        format.state = FormatState::undeclared;
        formats.push_back(std::move(format));
    }

    std::sort(formats.begin(), formats.end(), [](const Format& left, const Format& right) {
        return left.extension < right.extension;
    });
    return formats;
}

const char* formats_schema() { return kFormatsSchema; }

std::string formats_to_json(const std::vector<Format>& formats) {
    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.string(kFormatsSchema);
    writer.key("formats");
    writer.begin_array();
    for (const Format& format : formats) {
        writer.begin_object();
        writer.key("extension");
        writer.string(format.extension);
        writer.key("state");
        writer.string(format_state_name(format.state));
        // An undeclared format has no provider to name. The keys are omitted
        // rather than written empty so a consumer cannot read a component id
        // of "" as a component.
        if (format.composed()) {
            writer.key("capability");
            writer.string(format.capability);
            writer.key("component");
            writer.string(format.component);
            writer.key("version");
            writer.string(format.version);
            writer.key("artifact");
            writer.string(format.artifact);
        }
        writer.end_object();
    }
    writer.end_array();
    writer.end_object();
    return writer.text();
}

}  // namespace usd_geospatial
