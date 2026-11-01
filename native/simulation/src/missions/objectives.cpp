#include <ioj/sim/missions/objectives.h>

#include <algorithm>
#include <functional>

namespace ioj::sim {
Objectives::Objectives(std::size_t const count)
    : identifiers_(count)
    , display_names_(count)
    , assigned_teams_(count)
    , wins_level_(count, ObjectiveWinsLevel::No)
    , roots_(count)
    , leaf_ranges_(count)
    , composite_ranges_(count)
    , prerequisite_ranges_(count) {
    assert(count <= ObjectiveId::invalid_value);
    inactive_.reserve(count);
    for (std::uint16_t index{}; index < count; ++index) {
        inactive_.emplace_back(index);
    }
}
auto Objectives::is_completed(ObjectiveId const id) const -> bool {
    assert(is_valid(id));
    return std::ranges::contains(completed_, id);
}
auto Objectives::activate(std::span<ObjectiveMembershipIndex const> const positions)
    -> std::span<ObjectiveId const> {
    return move_members(inactive_, active_, positions);
}
auto Objectives::complete(std::span<ObjectiveMembershipIndex const> const positions)
    -> std::span<ObjectiveId const> {
    return move_members(active_, completed_, positions);
}
auto Objectives::compilation_view() -> CompilationView {
    return {identifiers_,
            display_names_,
            assigned_teams_,
            wins_level_,
            roots_,
            leaf_ranges_,
            composite_ranges_};
}
auto Objectives::prerequisites(ObjectiveId const id) const
    -> std::span<ObjectivePrerequisite const> {
    auto const range{at(prerequisite_ranges_, id)};
    return std::span<ObjectivePrerequisite const>{prerequisites_}.subspan(range.offset,
                                                                          range.count);
}
void Objectives::add_prerequisites(ObjectiveId const id,
                                   std::span<ObjectivePrerequisite const> const prerequisites) {
    at(prerequisite_ranges_, id) = {static_cast<std::uint32_t>(prerequisites_.size()),
                                    static_cast<std::uint32_t>(prerequisites.size())};
    prerequisites_.append_range(prerequisites);
}
auto Objectives::move_members(std::vector<ObjectiveId>& source,
                              std::vector<ObjectiveId>& destination,
                              std::span<ObjectiveMembershipIndex const> const positions)
    -> std::span<ObjectiveId const> {
    assert(std::ranges::adjacent_find(positions, std::greater_equal{}) == positions.end());
    auto const first{destination.size()};
    for (auto const position : positions) {
        assert(position < source.size());
        destination.push_back(source[position]);
    }

    // Reverse removal keeps the remaining positions tied to their original rows.
    for (auto index{positions.size()}; index > 0; --index) {
        source[positions[index - 1]] = source.back();
        source.pop_back();
    }

    return std::span<ObjectiveId const>{destination}.subspan(first);
}
}
