#pragma once

#include <ioj/levels/mission_definition.h>
#include <ioj/sim/missions/expr_composite_index.h>
#include <ioj/sim/missions/expr_leaves.h>
#include <ioj/sim/missions/expr_operators.h>
#include <ioj/sim/missions/mission_evaluation_context.h>
#include <ioj/sim/missions/objectives.h>
#include <ioj/sim/missions/team_mission_completion.h>

#include <expected>
#include <optional>

namespace ioj::sim {
struct SimClock;

class Mission {
  public:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    Mission() = default;

    static auto compile(levels::MissionDefinition const& definition,
                        std::span<levels::EntityId const> entities,
                        SimClock const& clock) -> std::expected<Mission, std::vector<std::string>>;

    void reset_progress();
    void evaluate(MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Inspection
    /* **************************************** */
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

    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;
    auto result(ExprHandle expression) const -> ExprResult;

  private:
    /* **************************************** */
    // Objective progression
    /* **************************************** */
    void activate_ready_objectives(MissionEvaluationContext const& inputs);
    void deactivate_expressions(std::span<ObjectiveId const> completed);

    /* **************************************** */
    // Objective evaluation
    /* **************************************** */
    void evaluate_objectives(MissionEvaluationContext const& inputs);
    auto root_outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;

    /* **************************************** */
    // Composite evaluation
    /* **************************************** */
    void evaluate_composites();

    /* **************************************** */
    // Leaf evaluation
    /* **************************************** */
    void resolve_leaf(LeafIndex index, ExprResult value);
    void evaluate_leaves(MissionEvaluationContext const& inputs);

    void evaluate_destroyed(std::span<LeafHandle const> run,
                            MissionEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<LeafHandle const> run, MissionEvaluationContext const& inputs);
    void evaluate_resolved(std::span<LeafHandle const> run);
    void evaluate_kills(std::span<LeafHandle const> run, MissionEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<LeafHandle const> run,
                                   MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Mission completion
    /* **************************************** */
    void finish(ObjectiveId winning_objective, MissionEvaluationContext const& inputs);

    Objectives objectives_;
    ExprOperators composites_;
    ExprLeaves leaves_;

    std::vector<TeamMissionCompletion> completions_;
};
}
