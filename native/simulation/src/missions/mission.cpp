#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/levels/mission_validation.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace ioj::sim {
/* **************************************** */
// Lifecycle
/* **************************************** */
auto Mission::compile(levels::MissionDefinition const& definition,
                      std::span<levels::EntityId const> const entities,
                      SimClock const& clock,
                      ExprManager& expressions,
                      Timers& timers,
                      LevelEventSchedule& events)
    -> std::expected<Mission, std::vector<std::string>> {
    auto errors{levels::validate_mission(definition, entities)};
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }

    auto const count{static_cast<std::uint16_t>(definition.objectives.size())};
    Mission result;
    result.objectives_ = Objectives{count};

    std::unordered_map<levels::ObjectiveName, ObjectiveId> objectives;
    std::unordered_map<levels::EntityId, LevelEntityIndex> entity_indices;

    auto const entity_count{entities.size()};
    for (LevelEntityIndex index{}; index < entity_count; ++index) {
        entity_indices.emplace(entities[index], index);
    }

    for (std::uint16_t index{}; index < count; ++index) {
        objectives.emplace(definition.objectives[index].id, ObjectiveId{index});
    }

    auto const columns{result.objectives_.compilation_view()};
    std::vector<ObjectivePrerequisite> prerequisites;

    for (std::uint16_t row{}; row < count; ++row) {
        ObjectiveId const owner{row};
        auto const& source{definition.objectives[row]};

        columns.identifiers[row] = source.id;
        columns.display_names[row] = source.title.empty() ? source.id.value : source.title;
        columns.assigned_teams[row] = source.team;
        columns.wins_level[row] = source.wins_level;

        prerequisites.clear();
        for (auto const& dependency : source.dependencies) {
            prerequisites.emplace_back(objectives.at(dependency.objective), dependency.outcome);
        }
        result.objectives_.add_prerequisites(owner, prerequisites);

        auto compiled{expressions.append(source.expressions,
                                         {entity_indices, objectives, clock, timers, events})};
        if (!compiled) {
            return std::unexpected{std::move(compiled.error())};
        }
        columns.leaf_ranges[row] = compiled->leaves;
        columns.composite_ranges[row] = compiled->composites;
        result.objectives_.add_activation_events(owner, compiled->start_events);

        for (auto const root : EnumTraits<ObjectiveOutcome>::values) {
            if (source.roots[root] != levels::absent_expression) {
                columns.roots[row][root] = compiled->handles[source.roots[root]];
            }
        }
    }

    return result;
}
void Mission::reset_progress() {
    objectives_.reset_progress();

    completions_.clear();
}
auto Mission::evaluate(MissionEvaluationContext const& inputs, ExprManager const& expressions)
    -> MissionChanges {
    auto const completed{evaluate_objectives(inputs, expressions)};
    auto const activated{activate_ready_objectives(inputs, expressions)};
    return {activated, completed};
}

/* **************************************** */
// Inspection
/* **************************************** */
auto Mission::team_completion(std::optional<Team> const team) const
    -> std::optional<TeamMissionCompletion> {
    std::optional<TeamMissionCompletion> result;
    if (team) {
        auto const found{std::ranges::find(completions_, *team, &TeamMissionCompletion::team)};
        if (found != completions_.end()) {
            result = *found;
        }
    }

    return result;
}
auto Mission::outcome(ObjectiveId const id, ExprManager const& expressions) const
    -> std::optional<ObjectiveOutcome> {
    return objectives_.is_completed(id) ? root_outcome(id, expressions) : std::nullopt;
}
/* **************************************** */
// Objective progression
/* **************************************** */
auto Mission::activate_ready_objectives(MissionEvaluationContext const& inputs,
                                        ExprManager const& expressions)
    -> std::span<ObjectiveId const> {
    FrameArray<ObjectiveMembershipIndex> ready{inputs.scratch};
    auto const inactive{objectives_.inactive()};
    auto const count{static_cast<ObjectiveMembershipIndex>(inactive.size())};
    for (ObjectiveMembershipIndex index{}; index < count; ++index) {
        auto const id{inactive[index]};
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select this objective's prerequisites.
        auto const prerequisites{objectives_.prerequisites(id)};
        if (std::ranges::all_of(prerequisites, [&](auto const prerequisite) {
                return outcome(prerequisite.objective(), expressions) == prerequisite.outcome();
            })) {
            ready.add(index);
        }
    }

    return objectives_.activate(ready.view());
}

/* **************************************** */
// Objective evaluation
/* **************************************** */
auto Mission::evaluate_objectives(MissionEvaluationContext const& inputs,
                                  ExprManager const& expressions) -> std::span<ObjectiveId const> {
    FrameArray<ObjectiveMembershipIndex> completed{inputs.scratch};
    ObjectiveId winning_objective{};
    auto const has_winner{mission_completed()};
    auto const active{objectives_.active()};
    auto const count{static_cast<ObjectiveMembershipIndex>(active.size())};

    for (ObjectiveMembershipIndex position{}; position < count; ++position) {
        auto const id{active[position]};
        if (auto const outcome{root_outcome(id, expressions)}) {
            completed.add(position);
            if (!has_winner && !winning_objective.is_valid() &&
                *outcome == ObjectiveOutcome::Passed &&
                objectives_.wins_level(id) == ObjectiveWinsLevel::Yes) {
                winning_objective = id;
            }
        }
    }

    auto const completed_ids{objectives_.complete(completed.view())};

    if (winning_objective.is_valid()) {
        finish(winning_objective, inputs);
    }
    return completed_ids;
}
auto Mission::root_outcome(ObjectiveId const id, ExprManager const& expressions) const
    -> std::optional<ObjectiveOutcome> {
    auto const& roots{objectives_.roots(id)};
    for (auto const outcome : EnumTraits<ObjectiveOutcome>::values) {
        if (expressions.result(roots[outcome]) == ExprResult::True) {
            return outcome;
        }
    }

    return std::nullopt;
}

/* **************************************** */
// Mission completion
/* **************************************** */
void Mission::finish(ObjectiveId const winning_objective, MissionEvaluationContext const& inputs) {
    auto const winner{objectives_.team(winning_objective)};
    completions_.push_back({winner,
                            MissionOutcome::Victory,
                            winning_objective,
                            ObjectiveOutcome::Passed,
                            inputs.tick});

    for (auto const team : inputs.participating_teams) {
        if (team != winner) {
            completions_.push_back({team,
                                    MissionOutcome::Defeat,
                                    winning_objective,
                                    ObjectiveOutcome::Passed,
                                    inputs.tick});
        }
    }
}
}
