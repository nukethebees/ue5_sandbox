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

    void begin(MissionEvaluationContext const& inputs);
    void tick(MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Inspection
    /* **************************************** */
    auto take_completions() -> std::vector<TeamMissionCompletion>;
    // Consume every pending completion; return the first for team, or empty without a match.
    auto take_team_completion(std::optional<Team> team) -> std::optional<TeamMissionCompletion>;
    auto objectives() const -> Objectives const& { return objectives_; }
    auto active_objectives() const -> std::span<ObjectiveId const> { return objectives_.active(); }
    auto inactive_objectives() const -> std::span<ObjectiveId const> {
        return objectives_.inactive();
    }
    auto completed_objectives() const -> std::span<ObjectiveId const> {
        return objectives_.completed();
    }

    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;
    auto team_outcome(Team team) const -> std::optional<MissionOutcome>;
    auto result(ExprHandle expression) const -> ExprResult;

    auto active_leaves() const -> std::span<LeafDataHandle const> { return leaves_.active; }
  private:
    /* **************************************** */
    // Objective progression
    /* **************************************** */
    void activate_ready_objectives(SimTick tick);
    auto prerequisites_met(ObjectiveId id) const -> bool;
    void activate(ObjectiveId id, SimTick tick);
    void deactivate_expressions(ObjectiveId id);

    /* **************************************** */
    // Objective evaluation
    /* **************************************** */
    void evaluate_objectives(MissionEvaluationContext const& inputs);
    auto root_outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;

    /* **************************************** */
    // Composite evaluation
    /* **************************************** */
    void evaluate_composites();
    auto evaluate_composite(ExprCompositeIndex index) const -> ExprResult;

    /* **************************************** */
    // Leaf evaluation
    /* **************************************** */
    auto expression_leaf(LeafDataHandle data_handle) const -> ExprLeafHandle;
    void resolve_leaf(LeafDataHandle data_handle, ExprResult value);
    void evaluate_leaves(MissionEvaluationContext const& inputs);

    void evaluate_run(LeafKind kind,
                      std::span<LeafDataHandle const> run,
                      MissionEvaluationContext const& inputs);
    void evaluate_destroyed(std::span<LeafDataHandle const> run,
                            MissionEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<LeafDataHandle const> run,
                          MissionEvaluationContext const& inputs);
    void evaluate_resolved(std::span<LeafDataHandle const> run);
    void evaluate_kills(std::span<LeafDataHandle const> run,
                        MissionEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<LeafDataHandle const> run,
                                   MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Mission completion
    /* **************************************** */
    void finish(ObjectiveId winning_objective, MissionEvaluationContext const& inputs);

    Objectives objectives_;
    ExprOperators composites_;
    ExprLeaves leaves_;

    std::optional<Team> winning_team_;
    EnumArray<Team, std::optional<MissionOutcome>> team_outcomes_;
    std::vector<TeamMissionCompletion> completions_;
    bool begun_{};
};
}
