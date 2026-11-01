#include <ioj/sim/missions/objectives.h>

#include <limits>

namespace ioj::sim {
Objectives::Objectives(std::size_t const count)
    : identifiers_(count)
    , display_names_(count)
    , assigned_teams_(count)
    , wins_level_(count, ObjectiveWinsLevel::No)
    , roots_(count)
    , leaf_ranges_(count)
    , composite_ranges_(count)
    , dependent_ranges_(count)
    , prerequisites_(count)
    , outcomes_(count)
    , activation_ticks_(count, SimTick{std::numeric_limits<SimTick::value_type>::max()})
    , membership_positions_(count) {
    inactive_.reserve(count);
    for (std::uint32_t index{}; index < count; ++index) {
        inactive_.emplace_back(index);
        membership_positions_[index] = index;
    }
}
auto Objectives::is_active(ObjectiveId const id) const -> bool {
    return activation_tick(id) != SimTick{std::numeric_limits<SimTick::value_type>::max()} &&
           !outcome(id);
}
void Objectives::activate(ObjectiveId const id, SimTick const tick) {
    assert(!outcome(id) &&
           activation_tick(id) == SimTick{std::numeric_limits<SimTick::value_type>::max()});

    remove_member(inactive_, id);
    at(membership_positions_, id) = static_cast<ObjectiveMembershipIndex>(active_.size());
    active_.push_back(id);
    at(activation_ticks_, id) = tick;
}
void Objectives::resolve(ObjectiveId const id, ObjectiveOutcome const outcome) {
    at(outcomes_, id) = outcome;
    completed_.push_back(id);
}
void Objectives::commit_completion(ObjectiveMembershipIndex const position) {
    auto const id{completed_[position]};
    remove_member(active_, id);
    at(membership_positions_, id) = position;
}
void Objectives::remove_member(std::vector<ObjectiveId>& collection, ObjectiveId const id) {
    auto const position{at(membership_positions_, id)};
    assert(position < collection.size() && collection[position] == id);
    collection[position] = collection.back();
    at(membership_positions_, collection.back()) = position;
    collection.pop_back();
}
}
