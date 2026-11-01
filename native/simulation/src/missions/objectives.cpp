#include <ioj/sim/missions/objectives.h>

#include <algorithm>

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
auto Objectives::activate(ObjectiveMembershipIndex const position) -> ObjectiveId {
    auto const id{remove_member(inactive_, position)};
    active_.push_back(id);
    return id;
}
auto Objectives::complete(ObjectiveMembershipIndex const position) -> ObjectiveId {
    auto const id{remove_member(active_, position)};
    completed_.push_back(id);
    return id;
}
auto Objectives::prerequisites(ObjectiveId const id) const
    -> std::span<ObjectivePrerequisite const> {
    auto const range{at(prerequisite_ranges_, id)};
    return std::span<ObjectivePrerequisite const>{prerequisites_}.subspan(range.offset,
                                                                          range.count);
}
void Objectives::add_prerequisite(ObjectiveId const id,
                                  ObjectiveId const prerequisite,
                                  ObjectiveOutcome const outcome) {
    auto& range{at(prerequisite_ranges_, id)};
    if (range.is_empty()) {
        range.offset = static_cast<std::uint32_t>(prerequisites_.size());
    }

    prerequisites_.emplace_back(prerequisite, outcome);
    ++range.count;
}
auto Objectives::remove_member(std::vector<ObjectiveId>& collection,
                               ObjectiveMembershipIndex const position) -> ObjectiveId {
    assert(position < collection.size());
    auto const id{collection[position]};
    collection[position] = collection.back();
    collection.pop_back();
    return id;
}
}
