#pragma once

#include <ioj/sim/missions/compiled_mission.h>
#include <ioj/sim/missions/mission_evaluation_context.h>
#include <ioj/sim/missions/team_completion.h>

namespace ioj::sim {
class Mission {
  public:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    Mission() = default;
    explicit Mission(CompiledMission definition);

    void begin(MissionEvaluationContext const& inputs);
    void tick(MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Inspection
    /* **************************************** */
    auto take_completions() -> std::vector<TeamCompletion>;
    auto objectives() const -> Objectives const& { return data_.objectives; }
    auto active_objectives() const -> std::span<ObjectiveId const> { return active_; }
    auto inactive_objectives() const -> std::span<ObjectiveId const> { return inactive_; }
    auto completed_objectives() const -> std::span<ObjectiveId const> { return completed_; }

    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;
    auto team_outcome(Team team) const -> std::optional<TeamOutcome>;
    auto result(ExprRef expression) const -> ExprResult;

    auto active_rules() const -> std::span<RuleId const> { return active_rules_; }
  private:
    /* **************************************** */
    // Objective progression
    /* **************************************** */
    auto active(ObjectiveId id) const -> bool;
    void remove_member(std::vector<ObjectiveId>& collection, ObjectiveId id);
    void activate(ObjectiveId id, SimTick tick);
    void complete(ObjectiveId id, ObjectiveOutcome outcome, SimTick tick);

    /* **************************************** */
    // Expression propagation
    /* **************************************** */
    void resolve(ExprRef expression, ExprResult value);
    void resolve_rule(RuleId rule, ExprResult value);
    void propagate();

    /* **************************************** */
    // Rule evaluation
    /* **************************************** */
    auto rule_leaf(RuleId rule) const -> std::uint32_t;
    auto pending_rule(RuleId rule) const -> bool;
    void evaluate(std::span<RuleId const> scheduled,
                  MissionEvaluationContext const& inputs,
                  bool activating = false);

    void evaluate_run(ConditionType type,
                      std::span<RuleId const> run,
                      MissionEvaluationContext const& inputs);
    void evaluate_destroyed(std::span<RuleId const> run, MissionEvaluationContext const& inputs);
    void evaluate_elapsed(std::span<RuleId const> run, MissionEvaluationContext const& inputs);
    void evaluate_resolved(std::span<RuleId const> run);
    void evaluate_kills(std::span<RuleId const> run, MissionEvaluationContext const& inputs);
    void evaluate_reached_position(std::span<RuleId const> run,
                                   MissionEvaluationContext const& inputs);

    /* **************************************** */
    // Settlement
    /* **************************************** */
    void settle(MissionEvaluationContext const& inputs);

    CompiledMission data_;
    std::vector<ObjectiveId> inactive_;
    std::vector<ObjectiveId> active_;
    std::vector<ObjectiveId> completed_;
    std::vector<RuleId> active_rules_;
    std::vector<RuleId> newly_active_;
    std::vector<ExprRef> propagation_;
    std::vector<ObjectiveId> pending_objectives_;
    std::vector<TeamResultIndex> candidate_results_;
    EnumArray<Team, std::optional<TeamOutcome>> team_outcomes_;
    std::vector<TeamCompletion> completions_;
    bool begun_{};
};
}
