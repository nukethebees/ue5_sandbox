#pragma once

#include <ioj/sim/missions/compiled_mission.h>
#include <ioj/sim/missions/mission_inputs.h>
#include <ioj/sim/missions/rule_run.h>
#include <ioj/sim/missions/team_completion.h>

namespace ioj::sim {
class Mission {
  public:
    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    Mission() = default;
    explicit Mission(CompiledMission definition);

    // Call once after initial spawns. Zero-duration roots can resolve during begin.
    void begin(MissionInputs const& inputs);
    // Requires begin() and nondecreasing simulation ticks.
    void tick(MissionInputs const& inputs);

    /* **************************************** */
    // Inspection
    /* **************************************** */
    auto take_completions() -> std::vector<TeamCompletion>;
    auto objectives() const -> Objectives const& { return data_.objectives; }
    auto active_objectives() const -> std::span<ObjectiveId const> { return active_; }
    auto inactive_objectives() const -> std::span<ObjectiveId const> { return inactive_; }
    auto completed_objectives() const -> std::span<ObjectiveId const> { return completed_; }

    // A missing outcome means unresolved, including inactive objectives. IDs must be in range.
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;
    auto team_outcome(Team team) const -> std::optional<TeamOutcome>;
    auto result(ExprRef expression) const -> ExprResult;

    auto active_rules() const -> std::span<RuleId const> { return active_rules_; }
    auto rule_runs() const -> std::span<RuleRun const> { return runs_; }
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
    void mark_dirty(ObjectiveId id);
    void resolve(ExprRef expression, ExprResult value);
    void resolve_rule(RuleId rule, ExprResult value);
    void propagate();

    /* **************************************** */
    // Rule evaluation
    /* **************************************** */
    auto rule_leaf(RuleId rule) const -> std::uint32_t;
    auto pending_rule(RuleId rule) const -> bool;
    void rebuild_runs();
    void evaluate(std::span<RuleId const> scheduled, MissionInputs const& inputs);

    // Each run contains only rules of the supplied condition type.
    void evaluate_run(ConditionType type, std::span<RuleId const> run, MissionInputs const& inputs);
    void evaluate_destroyed(std::span<RuleId const> run, MissionInputs const& inputs);
    void evaluate_elapsed(std::span<RuleId const> run, MissionInputs const& inputs);
    void evaluate_resolved(std::span<RuleId const> run);
    void evaluate_kills(std::span<RuleId const> run, MissionInputs const& inputs);
    void evaluate_reached_position(std::span<RuleId const> run, MissionInputs const& inputs);

    /* **************************************** */
    // Settlement
    /* **************************************** */
    void settle(MissionInputs const& inputs);

    CompiledMission data_;
    std::vector<ObjectiveId> inactive_, active_, completed_;
    std::vector<RuleId> active_rules_, newly_active_;
    std::vector<RuleRun> runs_;
    std::vector<ExprRef> propagation_;
    std::vector<ObjectiveId> dirty_;
    std::vector<std::uint32_t> candidate_results_;
    std::array<std::optional<TeamOutcome>, enum_count<Team>()> team_outcomes_{};
    std::vector<TeamCompletion> completions_;
    bool membership_changed_{};
    bool begun_{};
};
}
