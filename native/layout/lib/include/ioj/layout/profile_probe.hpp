#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace ioj::layout {

using ProbeSource = std::string;
using ProbeErrorMessage = std::string;
using ProbeSourceResult = std::expected<ProbeSource, ProbeErrorMessage>;

struct ExternalProbeRequest {
    auto operator==(ExternalProbeRequest const&) const -> bool = default;

    std::vector<std::string> spellings{};
    std::vector<std::string> headers{};
    std::vector<ProbeErrorMessage> diagnostics{};
};

// Complete-object probe subjects and known headers from the selected external group.
// Unsupported uses are omitted and diagnosed; no ABI equivalence is assumed.
auto external_probe_request(lispb::schema::TypeGraph const& types, lispb::schema::TypeId selected)
    -> ExternalProbeRequest;

// Produces standalone C++17 source. Compile in the target SDK; stdout is an importable profile.
auto profile_probe_source(std::span<std::string const> types, std::span<std::string const> headers)
    -> ProbeSourceResult;

} // namespace ioj::layout
