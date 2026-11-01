#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <limits>

namespace ioj::sim {
namespace mission_evaluation_detail {
auto entity(MissionEvaluationContext const& inputs, std::uint32_t const slot) -> EntityId {
    assert(slot < inputs.entities.size());
    return inputs.entities[slot];
}
auto destroyed(EntityId const id,
               EntityLedger const& ledger,
               std::span<LifeState const> const life_states) -> bool {
    assert(id.is_valid());
    auto const row{ledger.get_history_index(id)};
    assert(row < life_states.size());
    return life_states[row] != LifeState::Alive;
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
Mission::Mission(CompiledMission definition)
    : data_{std::move(definition)} {
    auto const count{data_.objectives.size()};
    inactive_.reserve(count);

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        inactive_.push_back(id);
        data_.objectives.membership_position(id) = index;
    }
}
void Mission::begin(MissionEvaluationContext const& inputs) {
    assert(!begun_);

    begun_ = true;
    auto const count{data_.objectives.size()};

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        if (data_.objectives.prerequisites(id) == 0) {
            activate(id, inputs.tick);
        }
    }

    std::ranges::sort(active_rules_);
    tick(inputs);
}
void Mission::tick(MissionEvaluationContext const& inputs) {
    assert(begun_);

    evaluate_rules(inputs);

    auto const first_completed{static_cast<ObjectiveMembershipIndex>(completed_.size())};
    evaluate_objectives();

    auto const completed_count{completed_.size()};
    for (auto position{first_completed}; position < completed_count; ++position) {
        auto const id{completed_[position]};
        complete(id, position, inputs.tick);
        if (!winning_team_ && data_.objectives.outcome(id) == ObjectiveOutcome::Passed &&
            data_.objectives.wins_level(id) == ObjectiveWinsLevel::Yes) {
            finish(id, inputs);
        }
    }

    std::erase_if(active_rules_, [&](RuleId const rule) { return !pending_rule(rule); });
    std::ranges::sort(active_rules_);
}

/* **************************************** */
// Inspection
/* **************************************** */
auto Mission::take_completions() -> std::vector<TeamMissionCompletion> {
    return std::exchange(completions_, {});
}
auto Mission::take_team_completion(std::optional<Team> const team)
    -> std::optional<TeamMissionCompletion> {
    std::optional<TeamMissionCompletion> result;
    if (team) {
        auto const found{std::ranges::find(completions_, *team, &TeamMissionCompletion::team)};
        if (found != completions_.end()) {
            result = *found;
        }
    }
    completions_.clear();
    return result;
}
auto Mission::outcome(ObjectiveId const id) const -> std::optional<ObjectiveOutcome> {
    return data_.objectives.outcome(id);
}
auto Mission::team_outcome(Team const team) const -> std::optional<MissionOutcome> {
    assert(static_cast<std::size_t>(team) < team_outcomes_.size());
    return team_outcomes_[team];
}
auto Mission::result(ExprHandle const expression) const -> ExprResult {
    if (!expression.is_valid()) {
        return ExprResult::False;
    }

    auto const& results{expression.leaf() ? data_.leaves.results : data_.composites.results};
    assert(expression.index() < results.size());
    return results[expression.index()];
}

/* **************************************** */
// Objective progression
/* **************************************** */
auto Mission::active(ObjectiveId const id) const -> bool {
    return data_.objectives.activation_tick(id) !=
               SimTick{std::numeric_limits<SimTick::value_type>::max()} &&
           !data_.objectives.outcome(id);
}
void Mission::remove_member(std::vector<ObjectiveId>& collection, ObjectiveId const id) {
    auto const position{data_.objectives.membership_position(id)};
    assert(position < collection.size() && collection[position] == id);
    collection[position] = collection.back();
    data_.objectives.membership_position(collection.back()) = position;
    collection.pop_back();
}
void Mission::activate(ObjectiveId const id, SimTick const tick) {
    assert(!data_.objectives.outcome(id) &&
           data_.objectives.activation_tick(id) ==
               SimTick{std::numeric_limits<SimTick::value_type>::max()});

    remove_member(inactive_, id);
    data_.objectives.membership_position(id) =
        static_cast<ObjectiveMembershipIndex>(active_.size());
    active_.push_back(id);
    data_.objectives.activation_tick(id) = tick;

    auto const leaves{data_.objectives.leaves(id)};
    auto const leaves_end{leaves.end()};

    for (ExprLeafHandle leaf{leaves.start()}; leaf < leaves_end; ++leaf) {
        auto const rule{data_.leaves.rules[leaf]};
        active_rules_.push_back(rule);
    }
}
void Mission::complete(ObjectiveId const id,
                       ObjectiveMembershipIndex const position,
                       SimTick const tick) {
    remove_member(active_, id);
    data_.objectives.membership_position(id) = position;
    auto const outcome{*data_.objectives.outcome(id)};

    auto const dependents{data_.objectives.dependents(id)};
    auto const dependents_end{dependents.end()};

    for (auto index{dependents.start()}; index < dependents_end; ++index) {
        auto const dependency{data_.dependents[index]};
        if (dependency.outcome == outcome) {
            auto& remaining{data_.objectives.prerequisites(dependency.dependent)};
            assert(remaining > 0);
            if (--remaining == 0) {
                activate(dependency.dependent, tick);
            }
        }
    }
}

/* **************************************** */
// Objective evaluation
/* **************************************** */
void Mission::evaluate_objectives() {
    for (auto const id : active_) {
        auto const composites{data_.objectives.composites(id)};
        auto const composites_end{composites.end()};
        for (ExprCompositeIndex index{composites.start()}; index < composites_end; ++index) {
            auto& value{data_.composites.results[index]};
            if (value == ExprResult::Pending) {
                value = evaluate_composite(index);
            }
        }

        auto const& roots{data_.objectives.roots(id)};
        for (auto const outcome : EnumTraits<ObjectiveOutcome>::values) {
            if (result(roots[outcome]) == ExprResult::True) {
                data_.objectives.outcome(id) = outcome;
                completed_.push_back(id);
                break;
            }
        }
    }
}
auto Mission::evaluate_composite(ExprCompositeIndex const index) const -> ExprResult {
    auto const all{data_.composites.types[index] == ExprType::All};
    auto const decisive{all ? ExprResult::False : ExprResult::True};
    auto value{all ? ExprResult::True : ExprResult::False};
    auto const children{data_.composites.child_ranges[index]};
    auto const children_end{children.end()};

    for (auto child{children.start()}; child < children_end; ++child) {
        auto const child_result{result(data_.children[child])};
        if (child_result == decisive) {
            return decisive;
        }
        if (child_result == ExprResult::Pending) {
            value = ExprResult::Pending;
        }
    }
    return value;
}

/* **************************************** */
// Rule evaluation
/* **************************************** */
auto Mission::rule_leaf(RuleId const rule) const -> ExprLeafHandle {
    return data_.rules.leaves[rule.type()][rule.index()];
}
auto Mission::pending_rule(RuleId const rule) const -> bool {
    auto const leaf{rule_leaf(rule)};
    return active(data_.leaves.owners[leaf]) && data_.leaves.results[leaf] == ExprResult::Pending;
}
void Mission::resolve_rule(RuleId const rule, ExprResult const value) {
    assert(value != ExprResult::Pending);
    data_.leaves.results[rule_leaf(rule)] = value;
}
void Mission::evaluate_rules(MissionEvaluationContext const& inputs) {
    auto const scheduled{std::span<RuleId const>{active_rules_}};
    assert(std::ranges::is_sorted(scheduled));
    auto const count{scheduled.size()};
    std::size_t first{};

    while (first < count) {
        auto const type{scheduled[first].type()};
        auto end{first + 1};
        while (end < count && scheduled[end].type() == type) {
            ++end;
        }

        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select the current type's rule range.
        evaluate_run(type, scheduled.subspan(first, end - first), inputs);
        first = end;
    }
}
void Mission::evaluate_run(RuleType const type,
                           std::span<RuleId const> const run,
                           MissionEvaluationContext const& inputs) {
    switch (type) {
        case RuleType::EntityDestroyed:
            evaluate_destroyed(run, inputs);
            break;
        case RuleType::TimeElapsed:
            evaluate_elapsed(run, inputs);
            break;
        case RuleType::ObjectiveResolved:
            evaluate_resolved(run);
            break;
        case RuleType::EntityKillsReached:
            evaluate_kills(run, inputs);
            break;
        case RuleType::EntityReachedPosition:
            evaluate_reached_position(run, inputs);
            break;
    }
}
void Mission::evaluate_destroyed(std::span<RuleId const> const run,
                                 MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const id{
                mission_evaluation_detail::entity(inputs, rules.destroyed_entities[rule.index()])};
            if (id.is_valid() &&
                mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
                resolve_rule(rule, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_elapsed(std::span<RuleId const> const run,
                               MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};

    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const activated{
                data_.objectives.activation_tick(data_.leaves.owners[rule_leaf(rule)])};
            assert(inputs.tick >= activated);
            if (inputs.tick.value() - activated.value() >= rules.durations[rule.index()].value()) {
                resolve_rule(rule, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_resolved(std::span<RuleId const> const run) {
    auto const& rules{data_.rules};

    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const outcome{data_.objectives.outcome(rules.observed_objectives[rule.index()])};
            if (outcome) {
                resolve_rule(rule,
                             *outcome == rules.observed_outcomes[rule.index()] ? ExprResult::True
                                                                               : ExprResult::False);
            }
        }
    }
}
void Mission::evaluate_kills(std::span<RuleId const> const run,
                             MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};

    for (auto const rule : run) {
        if (!pending_rule(rule)) {
            continue;
        }

        std::uint64_t kills{};
        auto const sources{rules.kill_sources[rule.index()]};
        auto const sources_end{sources.end()};
        for (auto index{sources.start()}; index < sources_end; ++index) {
            auto const id{mission_evaluation_detail::entity(inputs, rules.kill_entities[index])};
            if (id.is_valid()) {
                kills += inputs.ledger.get_kills(id);
            }
        }

        if (kills >= rules.kill_targets[rule.index()]) {
            resolve_rule(rule, ExprResult::True);
        }
    }
}
void Mission::evaluate_reached_position(std::span<RuleId const> const run,
                                        MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    assert(inputs.spatial && inputs.scratch);

    FrameArray<EntityId> ids{inputs.scratch};
    FrameArray<RuleId> live_rules{inputs.scratch};
    auto const count{static_cast<std::uint32_t>(run.size())};
    ids.set_num(count);
    live_rules.set_num(count);
    std::uint32_t live_count{};

    for (auto const rule : run) {
        if (!pending_rule(rule)) {
            continue;
        }

        auto const id{
            mission_evaluation_detail::entity(inputs, rules.reached_entities[rule.index()])};
        if (!id.is_valid()) {
            continue;
        }

        if (mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            resolve_rule(rule, ExprResult::False);
        } else {
            ids[live_count] = id;
            live_rules[live_count++] = rule;
        }
    }

    Vectors3f positions{*inputs.scratch};
    positions.add_uninitialised(live_count);
    auto const output{positions.get_view().resolve()};
    inputs.spatial->copy_entity_locations(
        std::span<EntityId const>{ids.data(), live_count}, output, inputs.scratch);

    for (std::uint32_t row{}; row < live_count; ++row) {
        auto const rule{live_rules[row]};
        auto const position{output[row]};
        auto const target{rules.positions[rule.index()]};
        Vector3d const position_double{position.x(), position.y(), position.z()};
        if (size_squared(position_double - target) <= rules.radius_squared[rule.index()]) {
            resolve_rule(rule, ExprResult::True);
        }
    }
}

/* **************************************** */
// Mission completion
/* **************************************** */
void Mission::finish(ObjectiveId const winning_objective, MissionEvaluationContext const& inputs) {
    auto const winner{data_.objectives.team(winning_objective)};
    winning_team_ = winner;
    team_outcomes_[winner] = MissionOutcome::Victory;
    completions_.push_back({winner,
                            MissionOutcome::Victory,
                            winning_objective,
                            ObjectiveOutcome::Passed,
                            inputs.tick});

    for (auto const team : inputs.participating_teams) {
        if (team != winner) {
            team_outcomes_[team] = MissionOutcome::Defeat;
            completions_.push_back({team,
                                    MissionOutcome::Defeat,
                                    winning_objective,
                                    ObjectiveOutcome::Passed,
                                    inputs.tick});
        }
    }
}
}
