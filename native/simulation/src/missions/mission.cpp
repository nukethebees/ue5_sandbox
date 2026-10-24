#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <limits>
#include <numeric>

namespace ioj::sim {
void Mission::initialise(CompiledMission definition) {
    *this = Mission{};
    data_ = std::move(definition);
    auto const count{data_.objectives.size()};
    inactive_.resize(count);
    std::iota(inactive_.begin(), inactive_.end(), ObjectiveId{});
    membership_positions_ = inactive_;
    outcomes_.resize(count);
    activation_ticks_.resize(count, std::numeric_limits<SimTick>::max());
    dirty_flags_.resize(count);
    prerequisites_.reserve(count);
    for (auto const& objective : data_.objectives) {
        prerequisites_.push_back(objective.prerequisites);
    }
}
void Mission::begin(MissionInputs const& inputs) {
    assert(!begun_);
    begun_ = true;
    auto const count{data_.objectives.size()};
    for (ObjectiveId id{}; id < count; ++id) {
        if (prerequisites_[id] == 0) {
            activate(id, inputs.tick);
        }
    }
    settle(inputs);
}
void Mission::tick(MissionInputs const& inputs) {
    assert(begun_);
    for (auto const run : runs_) {
        if (run.type == ConditionType::ObjectiveResolved) {
            continue;
        }
        evaluate(std::span<RuleId const>{active_rules_}.subspan(run.rules.offset, run.rules.count),
                 inputs);
    }
    settle(inputs);
}
auto Mission::outcome(ObjectiveId const id) const -> std::optional<ObjectiveOutcome> {
    assert(id < outcomes_.size());
    return outcomes_[id];
}
auto Mission::team_outcome(Team const team) const -> std::optional<TeamOutcome> {
    assert(static_cast<std::size_t>(team) < team_outcomes_.size());
    return team_outcomes_[static_cast<std::size_t>(team)];
}
auto Mission::take_completions() -> std::vector<TeamCompletion> {
    return std::exchange(completions_, {});
}
auto Mission::active(ObjectiveId const id) const -> bool {
    return activation_ticks_[id] != std::numeric_limits<SimTick>::max() && !outcomes_[id];
}
auto Mission::result(ExprRef const expression) const -> ExprResult {
    if (!expression.valid()) {
        return ExprResult::False;
    }
    auto const& results{expression.leaf() ? data_.leaves.results : data_.composites.results};
    assert(expression.index() < results.size());
    return results[expression.index()];
}
auto Mission::rule_leaf(RuleId const rule) const -> std::uint32_t {
    return data_.rules.leaves[static_cast<std::size_t>(rule.type())][rule.index()];
}
auto Mission::pending_rule(RuleId const rule) const -> bool {
    auto const leaf{rule_leaf(rule)};
    return active(data_.leaves.owners[leaf]) && data_.leaves.results[leaf] == ExprResult::Pending;
}
void Mission::remove_member(std::vector<ObjectiveId>& collection, ObjectiveId const id) {
    auto const position{membership_positions_[id]};
    assert(position < collection.size() && collection[position] == id);
    collection[position] = collection.back();
    membership_positions_[collection.back()] = position;
    collection.pop_back();
}
void Mission::activate(ObjectiveId const id, SimTick const tick) {
    assert(!outcomes_[id] && activation_ticks_[id] == std::numeric_limits<SimTick>::max());
    remove_member(inactive_, id);
    membership_positions_[id] = static_cast<std::uint32_t>(active_.size());
    active_.push_back(id);
    activation_ticks_[id] = tick;
    auto const& objective{data_.objectives[id]};
    for (auto leaf{objective.leaves.start()}; leaf < objective.leaves.end(); ++leaf) {
        auto const rule{data_.leaves.rules[leaf]};
        active_rules_.push_back(rule);
        newly_active_.push_back(rule);
    }
    membership_changed_ = true;
    for (auto node{objective.composites.start()}; node < objective.composites.end(); ++node) {
        if (data_.composites.remaining[node] == 0) {
            resolve({node},
                    data_.composites.types[node] == ExpressionType::All ? ExprResult::True
                                                                        : ExprResult::False);
        }
    }
    mark_dirty(id);
}
void Mission::mark_dirty(ObjectiveId const id) {
    if (!dirty_flags_[id]) {
        dirty_flags_[id] = 1;
        dirty_.push_back(id);
    }
}
void Mission::resolve(ExprRef const expression, ExprResult const value) {
    assert(value != ExprResult::Pending);
    auto& values{expression.leaf() ? data_.leaves.results : data_.composites.results};
    auto const index{expression.index()};
    if (values[index] != ExprResult::Pending) {
        return;
    }
    values[index] = value;
    propagation_.push_back(expression);
    mark_dirty(expression.leaf() ? data_.leaves.owners[index] : data_.composites.owners[index]);
    membership_changed_ = membership_changed_ || expression.leaf();
}
void Mission::propagate() {
    for (std::size_t index{}; index < propagation_.size(); ++index) {
        auto const child{propagation_[index]};
        auto const value{result(child)};
        auto const parents{child.leaf() ? data_.leaves.parents[child.index()]
                                        : data_.composites.parents[child.index()]};
        for (auto edge{parents.start()}; edge < parents.end(); ++edge) {
            auto const parent{data_.parents[edge]};
            if (data_.composites.results[parent] != ExprResult::Pending) {
                continue;
            }
            auto const all{data_.composites.types[parent] == ExpressionType::All};
            if ((all && value == ExprResult::False) || (!all && value == ExprResult::True)) {
                resolve({parent}, value);
            } else {
                auto& remaining{data_.composites.remaining[parent]};
                assert(remaining > 0);
                if (--remaining == 0) {
                    resolve({parent}, all ? ExprResult::True : ExprResult::False);
                }
            }
        }
    }
    propagation_.clear();
}
void Mission::complete(ObjectiveId const id, ObjectiveOutcome const outcome, SimTick const tick) {
    assert(active(id));
    remove_member(active_, id);
    membership_positions_[id] = static_cast<std::uint32_t>(completed_.size());
    completed_.push_back(id);
    outcomes_[id] = outcome;
    membership_changed_ = true;
    auto const& objective{data_.objectives[id]};
    for (auto index{objective.dependents.start()}; index < objective.dependents.end(); ++index) {
        auto const dependency{data_.dependents[index]};
        if (dependency.outcome == outcome) {
            auto& remaining{prerequisites_[dependency.dependent]};
            assert(remaining > 0);
            if (--remaining == 0) {
                activate(dependency.dependent, tick);
            }
        }
    }
    for (auto index{objective.observers.start()}; index < objective.observers.end(); ++index) {
        auto const leaf{data_.observers[index]};
        if (active(data_.leaves.owners[leaf])) {
            auto const rule{data_.leaves.rules[leaf]};
            resolve({ExprRef::leaf_bit | leaf},
                    data_.rules.observed_outcomes[rule.index()] == outcome ? ExprResult::True
                                                                           : ExprResult::False);
        }
    }
    for (auto index{objective.team_results.start()}; index < objective.team_results.end();
         ++index) {
        auto const mapping{data_.result_indices[index]};
        if (data_.team_results[mapping].outcome == outcome) {
            candidate_results_.push_back(mapping);
        }
    }
}
void Mission::rebuild_runs() {
    if (!membership_changed_) {
        return;
    }
    std::erase_if(active_rules_, [&](RuleId const rule) { return !pending_rule(rule); });
    std::ranges::sort(active_rules_);
    runs_.clear();
    auto const count{static_cast<std::uint32_t>(active_rules_.size())};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const type{active_rules_[index].type()};
        if (runs_.empty() || runs_.back().type != type) {
            runs_.push_back({type, {index, 0}});
        }
        ++runs_.back().rules.count;
    }
    membership_changed_ = false;
}
void Mission::evaluate(std::span<RuleId const> const scheduled, MissionInputs const& inputs) {
    auto const& rules{data_.rules};
    auto const history{inputs.ledger.get_unique_entities()};
    auto entity = [&](std::uint32_t const slot) {
        assert(slot < inputs.entities.size());
        return inputs.entities[slot];
    };
    auto dead = [&](EntityId const id) {
        assert(id.is_valid());
        auto const row{inputs.ledger.get_history_index(id)};
        assert(row < history.num());
        return history.life_state()[row] != LifeState::Alive;
    };
    auto finish = [&](RuleId const rule, ExprResult const value) {
        resolve({ExprRef::leaf_bit | rule_leaf(rule)}, value);
    };
    // Newly activated rules are sorted separately; ordinary ticks use cached runs.
    std::size_t first{};
    while (first < scheduled.size()) {
        auto const type{scheduled[first].type()};
        auto end{first + 1};
        while (end < scheduled.size() && scheduled[end].type() == type) {
            ++end;
        }
        auto const run{scheduled.subspan(first, end - first)};
        switch (type) {
            case ConditionType::EntityDestroyed:
                for (auto const rule : run) {
                    if (pending_rule(rule)) {
                        auto const id{entity(rules.destroyed_entities[rule.index()])};
                        if (id.is_valid() && dead(id)) {
                            finish(rule, ExprResult::True);
                        }
                    }
                }
                break;
            case ConditionType::TimeElapsed:
                for (auto const rule : run) {
                    if (pending_rule(rule)) {
                        auto const activated{
                            activation_ticks_[data_.leaves.owners[rule_leaf(rule)]]};
                        assert(inputs.tick >= activated);
                        if (inputs.tick - activated >= rules.durations[rule.index()]) {
                            finish(rule, ExprResult::True);
                        }
                    }
                }
                break;
            case ConditionType::ObjectiveResolved:
                // Resolve once on activation; subsequent changes arrive via reverse observers.
                for (auto const rule : run) {
                    if (pending_rule(rule)) {
                        auto const outcome{outcomes_[rules.observed_objectives[rule.index()]]};
                        if (outcome) {
                            finish(rule,
                                   *outcome == rules.observed_outcomes[rule.index()]
                                       ? ExprResult::True
                                       : ExprResult::False);
                        }
                    }
                }
                break;
            case ConditionType::EntityKillsReached:
                for (auto const rule : run) {
                    if (!pending_rule(rule)) {
                        continue;
                    }
                    std::uint64_t kills{};
                    auto const sources{rules.kill_sources[rule.index()]};
                    for (auto index{sources.start()}; index < sources.end(); ++index) {
                        auto const id{entity(rules.kill_entities[index])};
                        if (id.is_valid()) {
                            kills += inputs.ledger.get_kills(id);
                        }
                    }
                    if (kills >= rules.kill_targets[rule.index()]) {
                        finish(rule, ExprResult::True);
                    }
                }
                break;
            case ConditionType::EntityReachedPosition: {
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
                    auto const id{entity(rules.reached_entities[rule.index()])};
                    if (!id.is_valid()) {
                        continue;
                    }
                    if (dead(id)) {
                        finish(rule, ExprResult::False);
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
                    auto const x{static_cast<double>(position.x()) - target.x()};
                    auto const y{static_cast<double>(position.y()) - target.y()};
                    auto const z{static_cast<double>(position.z()) - target.z()};
                    if (x * x + y * y + z * z <= rules.radius_squared[rule.index()]) {
                        finish(rule, ExprResult::True);
                    }
                }
                break;
            }
        }
        first = end;
    }
}
void Mission::settle(MissionInputs const& inputs) {
    while (!newly_active_.empty() || !propagation_.empty() || !dirty_.empty()) {
        if (!newly_active_.empty()) {
            auto rules{std::exchange(newly_active_, {})};
            std::ranges::sort(rules);
            evaluate(rules, inputs);
        }
        propagate();
        auto dirty{std::exchange(dirty_, {})};
        std::ranges::sort(dirty);
        for (auto const id : dirty) {
            dirty_flags_[id] = 0;
            if (!active(id)) {
                continue;
            }
            auto const& roots{data_.objectives[id].roots};
            for (std::uint32_t root{}; root < roots.size(); ++root) {
                if (result(roots[root]) == ExprResult::True) {
                    complete(id, static_cast<ObjectiveOutcome>(root), inputs.tick);
                    break;
                }
            }
        }
    }
    std::ranges::sort(candidate_results_, [&](std::uint32_t const left, std::uint32_t const right) {
        auto const& a{data_.team_results[left]};
        auto const& b{data_.team_results[right]};
        if (a.team != b.team) {
            return a.team < b.team;
        }
        if (a.result != b.result) {
            return a.result == TeamOutcome::Defeat;
        }
        return left < right;
    });
    for (auto const index : candidate_results_) {
        auto const mapping{data_.team_results[index]};
        auto& outcome{team_outcomes_[static_cast<std::size_t>(mapping.team)]};
        if (!outcome) {
            outcome = mapping.result;
            completions_.push_back(
                {mapping.team, mapping.result, mapping.objective, mapping.outcome, inputs.tick});
        }
    }
    candidate_results_.clear();
    rebuild_runs();
}
}
