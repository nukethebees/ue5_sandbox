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
    inactive_.reserve(count);
    for (std::uint32_t index{}; index < count; ++index) {
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
auto Objectives::prerequisite_objectives(ObjectiveId const id) const
    -> std::span<ObjectiveId const> {
    auto const range{at(prerequisite_ranges_, id)};
    return std::span<ObjectiveId const>{prerequisite_objectives_}.subspan(range.offset,
                                                                          range.count);
}
auto Objectives::prerequisite_outcomes(ObjectiveId const id) const
    -> std::span<ObjectiveOutcome const> {
    auto const range{at(prerequisite_ranges_, id)};
    return std::span<ObjectiveOutcome const>{prerequisite_outcomes_}.subspan(range.offset,
                                                                             range.count);
}
void Objectives::add_prerequisite(ObjectiveId const id,
                                  ObjectiveId const prerequisite,
                                  ObjectiveOutcome const outcome) {
    auto& range{at(prerequisite_ranges_, id)};
    if (range.is_empty()) {
        range.offset = static_cast<std::uint32_t>(prerequisite_objectives_.size());
    }

    prerequisite_objectives_.push_back(prerequisite);
    prerequisite_outcomes_.push_back(outcome);
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
