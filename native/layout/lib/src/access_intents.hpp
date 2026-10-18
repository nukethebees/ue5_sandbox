#pragma once

#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/string_set.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ioj::layout::analysis_detail {

auto classified_useful_bytes(std::optional<std::uint64_t> const useful_bytes, bool const included)
    -> std::optional<std::uint64_t>;

auto logical_useful_bytes(std::optional<std::uint64_t> const classified_bytes,
                          std::uint64_t const multiplicity) -> std::optional<std::uint64_t>;

auto access_includes_read(AccessOperation const operation) -> bool;

auto access_includes_write(AccessOperation const operation) -> bool;

auto accept_unique_access(StringSet& names,
                          std::span<AccessIntent const> const accepted,
                          std::vector<Diagnostic>& diagnostics,
                          AccessIntent const& access,
                          std::string_view const member_kind) -> bool;

auto access_intents_match(std::span<AccessIntent const> const first,
                          std::span<AccessIntent const> const second) -> bool;

}
