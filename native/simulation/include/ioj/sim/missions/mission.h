#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/levels/mission_definition.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/sim_clock.h>

#include <array>
#include <cassert>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ioj::sim {
class EntityLedger;
struct SpatialQueryManager;
using ObjectiveId = std::uint32_t;

struct ExprRef {
    std::uint32_t value{0xffffffffu};
    static constexpr std::uint32_t leaf_bit{0x80000000u};
    auto valid() const -> bool { return value != 0xffffffffu; }
    auto leaf() const -> bool { return (value & leaf_bit) != 0; }
    auto index() const -> std::uint32_t { return value & ~leaf_bit; }
    auto operator<=>(ExprRef const&) const = default;
};

enum class ConditionType : std::uint8_t {
    EntityDestroyed,
    EntityReachedPosition,
    TimeElapsed,
    ObjectiveResolved,
    EntityKillsReached
};
struct RuleId {
    std::uint32_t value{};
    static auto make(ConditionType const type, std::uint32_t const index) -> RuleId {
        assert(index < (1u << 24));
        return {(static_cast<std::uint32_t>(type) << 24) | index};
    }
    auto type() const -> ConditionType { return static_cast<ConditionType>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0xffffffu; }
    auto operator<=>(RuleId const&) const = default;
};
struct RuleRun {
    ConditionType type{};
    IndexSpan rules;
};

struct MissionComposites {
    std::vector<ExprResult> results;
    std::vector<ExpressionType> types;
    std::vector<ObjectiveId> owners;
    std::vector<IndexSpan> parents;
    std::vector<IndexSpan> children;
    std::vector<std::uint32_t> remaining;
};
struct MissionLeaves {
    std::vector<ExprResult> results;
    std::vector<ObjectiveId> owners;
    std::vector<IndexSpan> parents;
    std::vector<RuleId> rules;
};
struct MissionRules {
    std::array<std::vector<std::uint32_t>, 5> leaves;
    std::vector<std::uint32_t> destroyed_entities;
    std::vector<std::uint32_t> reached_entities;
    std::vector<Vector3d> positions;
    std::vector<double> radius_squared;
    std::vector<SimTick> durations;
    std::vector<ObjectiveId> observed_objectives;
    std::vector<ObjectiveOutcome> observed_outcomes;
    std::vector<IndexSpan> kill_sources;
    std::vector<std::uint32_t> kill_targets;
    std::vector<std::uint32_t> kill_entities;
};
struct CompiledObjective {
    std::string id;
    std::string title;
    Team team{};
    std::array<ExprRef, 3> roots;
    IndexSpan leaves;
    IndexSpan composites;
    std::uint32_t prerequisites{};
    IndexSpan dependents;
    IndexSpan observers;
    IndexSpan team_results;
};
struct ActivationNotification {
    ObjectiveId dependent{};
    ObjectiveOutcome outcome{};
};
struct MissionTeamResult {
    Team team{};
    ObjectiveId objective{};
    ObjectiveOutcome outcome{};
    TeamOutcome result{};
};
struct CompiledMission {
    std::vector<CompiledObjective> objectives;
    MissionComposites composites;
    MissionLeaves leaves;
    MissionRules rules;
    std::vector<ExprRef> children;
    std::vector<std::uint32_t> parents;
    std::vector<ActivationNotification> dependents;
    std::vector<std::uint32_t> observers;
    std::vector<MissionTeamResult> team_results;
    std::vector<std::uint32_t> result_indices;
};

auto compile_mission(levels::MissionDefinition const& definition,
                     std::span<levels::EntityId const> entities,
                     SimClock const& clock)
    -> std::expected<CompiledMission, std::vector<std::string>>;

struct MissionInputs {
    SimTick tick{};
    EntityLedger const& ledger;
    // Stable authored entity slots; unspawned slots contain the null EntityId sentinel.
    std::span<EntityId const> entities;
    SpatialQueryManager const* spatial{};
    FrameMemoryResource* scratch{};
};
struct TeamCompletion {
    Team team{};
    TeamOutcome outcome{};
    ObjectiveId objective{};
    ObjectiveOutcome objective_outcome{};
    SimTick tick{};
};

class Mission {
  public:
    void initialise(CompiledMission definition);
    // Call once after initial spawns. Zero-duration roots can resolve during begin.
    void begin(MissionInputs const& inputs);
    void tick(MissionInputs const& inputs);
    auto take_completions() -> std::vector<TeamCompletion>;
    auto objectives() const -> std::span<CompiledObjective const> { return data_.objectives; }
    auto active_objectives() const -> std::span<ObjectiveId const> { return active_; }
    auto inactive_objectives() const -> std::span<ObjectiveId const> { return inactive_; }
    auto completed_objectives() const -> std::span<ObjectiveId const> { return completed_; }
    auto outcome(ObjectiveId id) const -> std::optional<ObjectiveOutcome>;
    auto team_outcome(Team team) const -> std::optional<TeamOutcome>;
    auto result(ExprRef expression) const -> ExprResult;
    auto active_rules() const -> std::span<RuleId const> { return active_rules_; }
    auto rule_runs() const -> std::span<RuleRun const> { return runs_; }
  private:
    void activate(ObjectiveId id, SimTick tick);
    void complete(ObjectiveId id, ObjectiveOutcome outcome, SimTick tick);
    void resolve(ExprRef expression, ExprResult result);
    void propagate();
    void rebuild_runs();
    void evaluate(std::span<RuleId const> rules, MissionInputs const& inputs);
    void settle(MissionInputs const& inputs);
    void mark_dirty(ObjectiveId id);
    auto pending_rule(RuleId rule) const -> bool;
    auto active(ObjectiveId id) const -> bool;
    auto rule_leaf(RuleId rule) const -> std::uint32_t;
    void remove_member(std::vector<ObjectiveId>& collection, ObjectiveId id);

    CompiledMission data_;
    std::vector<ObjectiveId> inactive_, active_, completed_;
    std::vector<std::uint32_t> membership_positions_;
    std::vector<std::optional<ObjectiveOutcome>> outcomes_;
    std::vector<SimTick> activation_ticks_;
    std::vector<std::uint32_t> prerequisites_;
    std::vector<RuleId> active_rules_, newly_active_;
    std::vector<RuleRun> runs_;
    std::vector<ExprRef> propagation_;
    std::vector<ObjectiveId> dirty_;
    std::vector<std::uint8_t> dirty_flags_;
    std::vector<std::uint32_t> candidate_results_;
    std::array<std::optional<TeamOutcome>, enum_count<Team>()> team_outcomes_{};
    std::vector<TeamCompletion> completions_;
    bool membership_changed_{};
    bool begun_{};
};
}
