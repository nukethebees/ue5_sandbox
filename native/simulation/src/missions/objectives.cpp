#include <ioj/sim/missions/objectives.h>

namespace ioj::sim {
Objectives::Objectives(std::size_t const count)
    : identifiers_(count)
    , display_names_(count)
    , assigned_teams_(count)
    , wins_level_(count, ObjectiveWinsLevel::No)
    , roots_(count)
    , leaf_ranges_(count)
    , composite_ranges_(count)
    , prerequisite_ranges_(count)
    , membership_positions_(count) {
    assert(count <= ObjectiveId::invalid_value);
    inactive_.reserve(count);
    for (std::uint16_t index{}; index < count; ++index) {
        inactive_.emplace_back(index);
        membership_positions_[index] = index;
    }
}
auto Objectives::is_completed(ObjectiveId const id) const -> bool {
    auto const position{at(membership_positions_, id)};
    return position < completed_.size() && completed_[position] == id;
}
void Objectives::activate(ObjectiveId const id) {
    remove_member(inactive_, id);
    at(membership_positions_, id) = static_cast<ObjectiveMembershipIndex>(active_.size());
    active_.push_back(id);
}
void Objectives::complete(ObjectiveId const id) {
    remove_member(active_, id);
    at(membership_positions_, id) = static_cast<ObjectiveMembershipIndex>(completed_.size());
    completed_.push_back(id);
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
void Objectives::remove_member(std::vector<ObjectiveId>& collection, ObjectiveId const id) {
    auto const position{at(membership_positions_, id)};
    assert(position < collection.size() && collection[position] == id);
    collection[position] = collection.back();
    at(membership_positions_, collection.back()) = position;
    collection.pop_back();
}
}
