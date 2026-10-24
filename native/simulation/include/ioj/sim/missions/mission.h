#pragma once

#include <ioj/sim/missions/compiled_mission.h>
#include <ioj/sim/missions/mission_inputs.h>
#include <ioj/sim/missions/rule_run.h>
#include <ioj/sim/missions/team_completion.h>

namespace ioj::sim {
class Mission {
  public:
    void initialise(CompiledMission definition);
    // Call once after initial spawns. Zero-duration roots can resolve during begin.
    void begin(MissionInputs const& inputs);
    // Requires begin() and nondecreasing simulation ticks.
    void tick(MissionInputs const& inputs);
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
    void activate(ObjectiveId id, SimTick tick);
    void complete(ObjectiveId id, ObjectiveOutcome outcome, SimTick tick);
    void resolve(ExprRef expression, ExprResult value);
    void propagate();
    void rebuild_runs();
    void evaluate(std::span<RuleId const> scheduled, MissionInputs const& inputs);
    void settle(MissionInputs const& inputs);
    void mark_dirty(ObjectiveId id);
    auto pending_rule(RuleId rule) const -> bool;
    auto active(ObjectiveId id) const -> bool;
    auto rule_leaf(RuleId rule) const -> std::uint32_t;
    void remove_member(std::vector<ObjectiveId>& collection, ObjectiveId id);

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
