#include "ioj/sim/capital_ship_queries.h"

#include <algorithm>
#include <cstddef>

namespace ioj::sim {
auto find_capital_ship_index(std::span<RegistryEntityHandle const> const handles,
                             RegistryEntityHandle const handle) noexcept
    -> std::optional<std::int32_t> {
    auto const found{std::ranges::find(handles, handle)};
    if (found == handles.end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(found - handles.begin());
}

auto find_first_capital_ship_on_team(std::span<std::byte const> const teams,
                                     Team const team) noexcept -> std::optional<std::int32_t> {
    auto const team_value{static_cast<std::uint8_t>(team)};
    auto const found{std::ranges::find_if(teams, [team_value](auto const value) {
        return std::to_integer<std::uint8_t>(value) == team_value;
    })};
    if (found == teams.end()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(found - teams.begin());
}

auto collect_capitals_without_targets(std::span<RegistryEntityHandle const> const target_handles,
                                      ml::FrameArray<std::int32_t>& output_indices)
    -> std::int32_t {
    auto const capital_count{static_cast<std::int32_t>(target_handles.size())};
    output_indices.reserve(capital_count);
    for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        if (!target_handles[static_cast<std::size_t>(capital_index)].is_null()) {
            continue;
        }
        output_indices.add(capital_index);
    }
    return output_indices.num();
}
} // namespace ioj::sim
