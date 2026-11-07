#pragma once

#include <ioj/levels/mission_definition.h>
#include <ioj/sim/missions/mission_changes.h>
#include <ioj/sim/missions/mission_evaluation_context.h>
#include <ioj/sim/missions/objectives.h>
#include <ioj/sim/missions/team_mission_completion.h>

#include <expected>
#include <optional>
#include <span>

namespace ioj::sim {
class ExprManager;
struct SimClock;
class Timers;

class Mission {
  public:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    Mission() = default;

    static auto compile(levels::MissionDefinition const& definition,
                        std::span<levels::EntityId const> entities,
                        SimClock const& clock,
                        ExprManager& expressions,
                        Timers& timers) -> std::expected<Mission, std::vector<std::string>>;

    void reset_progress();
    auto evaluate(MissionEvaluationContext const& inputs, ExprManager const& expressions)
        -> MissionChanges;

    /* **************************************** */
    // Inspection
    /* **************************************** */
    [[nodiscard]] auto mission_completed() const noexcept -> bool { return !completions_.empty(); }
    auto completions() const -> std::span<TeamMissionCompletion const> { return completions_; }
    auto team_completion(std::optional<Team> team) const -> std::optional<TeamMissionCompletion>;
    auto objectives() const -> Objectives const& { return objectives_; }
    auto active_objectives() const -> std::span<ObjectiveId const> { return objectives_.active(); }
    auto inactive_objectives() const -> std::span<ObjectiveId const> {
        return objectives_.inactive();
    }
    auto completed_objectives() const -> std::span<ObjectiveId const> {
        return objectives_.completed();
    }

    auto outcome(ObjectiveId id, ExprManager const& expressions) const
        -> std::optional<ObjectiveOutcome>;
  private:
    /* **************************************** */
    // Objective progression
    /* **************************************** */
    auto activate_ready_objectives(MissionEvaluationContext const& inputs,
                                   ExprManager const& expressions) -> std::span<ObjectiveId const>;

    /* **************************************** */
    // Objective evaluation
    /* **************************************** */
    auto evaluate_objectives(MissionEvaluationContext const& inputs, ExprManager const& expressions)
        -> std::span<ObjectiveId const>;
    auto root_outcome(ObjectiveId id, ExprManager const& expressions) const
        -> std::optional<ObjectiveOutcome>;

    /* **************************************** */
    // Mission completion
    /* **************************************** */
    void finish(ObjectiveId winning_objective, MissionEvaluationContext const& inputs);

    Objectives objectives_;

    std::vector<TeamMissionCompletion> completions_;
};
}
