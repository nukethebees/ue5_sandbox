#pragma once

#include <lispb/schema/type_graph.h>

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace ioj::layout {

struct ExternalProbeRequest {
    std::vector<std::string> spellings;
    std::vector<std::string> headers;
    std::vector<std::string> diagnostics;
};

// Complete-object probe subjects and known headers from the selected external group.
// Unsupported uses are omitted and diagnosed; no ABI equivalence is assumed.
auto external_probe_request(lispb::schema::TypeGraph const& types, lispb::schema::TypeId selected)
    -> ExternalProbeRequest;

// Produces standalone C++17 source. Compile in the target SDK; stdout is an importable profile.
auto profile_probe_source(std::span<std::string const> types, std::span<std::string const> headers)
    -> std::expected<std::string, std::string>;

} // namespace ioj::layout
