#pragma once

#include "checked_arithmetic.hpp"
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/string_set.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::layout::analysis_detail {

inline auto classified_useful_bytes(std::optional<std::uint64_t> const useful_bytes,
                                    bool const included) -> std::optional<std::uint64_t> {
    return included ? useful_bytes : std::optional<std::uint64_t>{std::uint64_t{}};
}

inline auto logical_useful_bytes(std::optional<std::uint64_t> const classified_bytes,
                                 std::uint64_t const multiplicity) -> std::optional<std::uint64_t> {
    if (multiplicity == 0 || !classified_bytes.has_value()) {
        return std::nullopt;
    }
    return checked_multiply(*classified_bytes, multiplicity);
}

inline auto access_includes_read(AccessOperation const operation) -> bool {
    return operation != AccessOperation::write;
}

inline auto access_includes_write(AccessOperation const operation) -> bool {
    return operation != AccessOperation::read;
}

inline auto accept_unique_access(StringSet& names,
                                 std::span<AccessIntent const> const accepted,
                                 std::vector<Diagnostic>& diagnostics,
                                 AccessIntent const& access,
                                 std::string_view const member_kind) -> bool {
    if (names.insert(access.name).second) {
        return true;
    }
    auto const existing{std::ranges::find(accepted, access.name, &AccessIntent::name)};
    if (existing != accepted.end() && existing->operation != access.operation) {
        diagnostics.push_back({DiagnosticSeverity::error,
                               "Selected " + std::string{member_kind} + " '" + access.name +
                                   "' has conflicting access classifications; the first is used."});
    }
    return false;
}

inline auto access_intents_match(std::span<AccessIntent const> const first,
                                 std::span<AccessIntent const> const second) -> bool {
    if (first.size() != second.size()) {
        return false;
    }
    return std::ranges::all_of(first, [&](AccessIntent const& access) {
        auto const match{std::ranges::find(second, access.name, &AccessIntent::name)};
        return match != second.end() && match->operation == access.operation;
    });
}

}
