#include <ioj/sim/expressions/condition_manager.h>

#include <ioj/frame_array.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/levels/expression.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace ioj::sim {
namespace expr_compilation_detail {
struct ExpressionVisit {
    levels::ExprIndex node;
    std::size_t next_child{};
};

auto expression_order(std::span<levels::Expression const> const expressions)
    -> std::vector<levels::ExprIndex> {
    auto const count{expressions.size()};
    std::vector<levels::ExprIndex> order;
    order.reserve(count);
    std::vector<bool> visited(count);
    std::vector<ExpressionVisit> stack;

    for (levels::ExprIndex index{}; index < count; ++index) {
        if (visited[index]) {
            continue;
        }

        visited[index] = true;
        stack.emplace_back(index, 0);

        while (!stack.empty()) {
            auto& [node, next_child]{stack.back()};
            // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select the current traversal node.
            auto const children{expressions[node].children()};
            if (next_child == children.size()) {
                order.push_back(node);
                stack.pop_back();
            } else {
                auto const child{children[next_child++]};
                if (!visited[child]) {
                    visited[child] = true;
                    stack.emplace_back(child, 0);
                }
            }
        }
    }

    return order;
}
}

namespace expr_evaluation_detail {
auto entity(ExprEvaluationContext const& inputs, LevelEntityRef const slot) -> EntityId {
    assert(slot.index() < inputs.entities.size());
    return inputs.entities[slot.index()];
}
auto destroyed(EntityId const id,
               EntityLedger const& ledger,
               std::span<LifeState const> const life_states) -> bool {
    auto const row{ledger.history_index(id)};
    assert(row < life_states.size());
    return life_states[row] != LifeState::Alive;
}
}

/* **************************************** */
// Compilation
/* **************************************** */
void ConditionManager::append(std::span<levels::Expression const> const expressions,
                              ExprCompilationContext const& ctx,
                              std::span<ExprHandle> const references) {
    assert(references.size() == expressions.size());

    auto& leaves{leaves_};
    auto& composites{composites_};
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };
    auto const order{expr_compilation_detail::expression_order(expressions)};

    for (auto const index : order) {
        auto const& expression{expressions[index]};
        auto const kind{expression.type()};
        if (is_composite(kind)) {
            auto const composite{size(composites.results)};
            references[index] = ExprHandle{composite, kind};
            composites.results.push_back(false);
            composites.kinds.push_back(kind);

            // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select this expression's children.
            auto const children{expression.children()};
            composites.child_ranges.push_back({size(composites.children), size(children)});
            for (auto const child : children) {
                composites.children.push_back(references[child]);
            }

            continue;
        }

        auto const leaf{static_cast<LeafIndex>(leaves.num_leaves())};
        leaves.results.push_back(false);

        LeafDataIndex data_index{};
        switch (kind) {
            case ExprKind::EntityDestroyed: {
                auto const& condition{std::get<levels::EntityDestroyedExpr>(expression.payload)};

                data_index = static_cast<LeafDataIndex>(leaves.destroyed_entities.size());
                leaves.destroyed_entities.push_back(ctx.entities.at(condition.entity));
                break;
            }

            case ExprKind::EntityReachedPosition: {
                auto const& condition{
                    std::get<levels::EntityReachedPositionExpr>(expression.payload)};

                data_index = static_cast<LeafDataIndex>(leaves.reached_entities.size());
                leaves.reached_entities.push_back(ctx.entities.at(condition.entity));
                leaves.positions.push_back(condition.position.get());
                leaves.radius_squared.push_back(condition.radius.get() * condition.radius.get());
                break;
            }

            case ExprKind::TimerElapsed: {
                data_index = static_cast<LeafDataIndex>(leaves.timers.size());
                auto const timer_index{
                    std::get<levels::TimerElapsedExpr>(expression.payload).timer};
                auto const timer{ctx.timers[timer_index]};
                assert(timer.is_valid());
                leaves.timers.push_back(timer);
                break;
            }

            case ExprKind::LatchedTrue:
                data_index = size(leaves.true_latches);
                leaves.true_latches.emplace_back(
                    std::get<levels::LatchedTrueExpr>(expression.payload).latch);
                break;
            case ExprKind::LatchedFalse:
                data_index = size(leaves.false_latches);
                leaves.false_latches.emplace_back(
                    std::get<levels::LatchedFalseExpr>(expression.payload).latch);
                break;

            case ExprKind::EntityKillsReached: {
                auto const& condition{std::get<levels::EntityKillsReachedExpr>(expression.payload)};
                data_index = static_cast<LeafDataIndex>(leaves.kill_sources.size());

                leaves.kill_sources.push_back(
                    {size(leaves.kill_entities), size(condition.entities)});
                leaves.kill_targets.push_back(condition.kills);

                for (auto const& entity : condition.entities) {
                    leaves.kill_entities.push_back(ctx.entities.at(entity));
                }

                break;
            }

            default:
                std::unreachable();
        }

        references[index] = ExprHandle{leaf, kind};
        leaves.data_handles.emplace_back(data_index, kind);
        leaves.leaves_by_kind[kind].push_back(leaf);
    }
}
auto ConditionManager::add_timer_elapsed(TimerHandle const timer) -> ExprHandle {
    assert(timer.is_valid());
    auto const leaf{static_cast<LeafIndex>(leaves_.num_leaves())};
    auto const data{static_cast<LeafDataIndex>(leaves_.timers.size())};
    leaves_.timers.push_back(timer);
    leaves_.results.push_back(false);
    leaves_.data_handles.emplace_back(data, ExprKind::TimerElapsed);
    leaves_.leaves_by_kind[ExprKind::TimerElapsed].push_back(leaf);
    return ExprHandle{leaf, ExprKind::TimerElapsed};
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void ConditionManager::evaluate(ExprEvaluationContext const& inputs) {
    std::ranges::fill(leaves_.results, ExprResult{});
    evaluate_leaves(inputs);
    evaluate_composites();
    evaluate_latches();
}

/* **************************************** */
// Inspection
/* **************************************** */
auto ConditionManager::result(ExprHandle const expression) const -> bool {
    assert(expression.is_valid());

    auto const& results{is_composite(expression.kind()) ? composites_.results : leaves_.results};
    auto const index{expression.index()};
    assert(index < results.size());
    return results[index];
}
auto ConditionManager::children(ExprHandle const expression) const -> std::span<ExprHandle const> {
    assert(expression.is_valid() && is_composite(expression.kind()));
    auto const index{expression.index()};
    assert(index < composites_.child_ranges.size());

    auto const range{composites_.child_ranges[index]};
    return std::span{composites_.children}.subspan(range.offset, range.count);
}
auto ConditionManager::destroyed_entity(ExprHandle const expression) const -> LevelEntityRef {
    assert(expression.is_valid() && expression.kind() == ExprKind::EntityDestroyed);
    auto const index{expression.index()};
    assert(index < leaves_.data_handles.size());

    return leaves_.destroyed_entities[leaves_.data_handles[index].index()];
}
auto ConditionManager::latched_true_value(ExprHandle const expression) const -> ExprHandle {
    assert(expression.is_valid() && expression.kind() == ExprKind::LatchedTrue);
    auto const index{expression.index()};
    assert(index < leaves_.data_handles.size());
    auto const latch{leaves_.true_latches[leaves_.data_handles[index].index()]};
    assert(is_valid(latch));
    return latch_inputs_[latch.index()].value;
}

/* **************************************** */
// Composite evaluation
/* **************************************** */
void ConditionManager::evaluate_composites() {
    auto const count{composites_.results.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const kind{composites_.kinds[index]};
        auto const range{composites_.child_ranges[index]};
        auto const children{std::span{composites_.children}.subspan(range.offset, range.count)};
        bool value{kind == ExprKind::All};
        if (kind == ExprKind::Not) {
            value = !result(children.front());
        } else {
            for (auto const child : children) {
                auto const operand{result(child)};
                switch (kind) {
                    case ExprKind::All:
                        value &= operand;
                        break;
                    case ExprKind::Any:
                        value |= operand;
                        break;
                    case ExprKind::Xor:
                        value ^= operand;
                        break;
                    default:
                        std::unreachable();
                }
            }
        }

        composites_.results[index] = value;
    }
}
void ConditionManager::evaluate_leaves(ExprEvaluationContext const& inputs) {
    evaluate_destroyed(leaves_.leaves_by_kind[ExprKind::EntityDestroyed], inputs);
    evaluate_elapsed(leaves_.leaves_by_kind[ExprKind::TimerElapsed], inputs);
    evaluate_kills(leaves_.leaves_by_kind[ExprKind::EntityKillsReached], inputs);

    auto const& positions{leaves_.leaves_by_kind[ExprKind::EntityReachedPosition]};
    if (!positions.empty()) {
        evaluate_reached_position(positions, inputs);
    }

    for (auto const index : leaves_.leaves_by_kind[ExprKind::LatchedTrue]) {
        leaves_.results[index] =
            latch_state(leaves_.true_latches[leaves_.data_handles[index].index()]) ==
            LatchState::True;
    }

    for (auto const index : leaves_.leaves_by_kind[ExprKind::LatchedFalse]) {
        leaves_.results[index] =
            latch_state(leaves_.false_latches[leaves_.data_handles[index].index()]) ==
            LatchState::False;
    }
}
void ConditionManager::evaluate_destroyed(std::span<LeafIndex const> const run,
                                          ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.history().life_state()};

    for (auto const leaf : run) {
        auto const data{leaves.data_handles[leaf].index()};
        auto const id{expr_evaluation_detail::entity(inputs, leaves.destroyed_entities[data])};

        if (id.is_valid() && expr_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            leaves_.results[leaf] = true;
        }
    }
}
void ConditionManager::evaluate_elapsed(std::span<LeafIndex const> const run,
                                        ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const leaf : run) {
        auto const data{leaves.data_handles[leaf].index()};

        leaves_.results[leaf] = inputs.timers.elapsed(leaves.timers[data]);
    }
}
void ConditionManager::evaluate_kills(std::span<LeafIndex const> const run,
                                      ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const leaf : run) {
        auto const data{leaves.data_handles[leaf].index()};
        KillCount kills{};
        auto const sources{leaves.kill_sources[data]};
        auto const sources_end{sources.end()};
        for (auto index{sources.start()}; index < sources_end; ++index) {
            auto const id{expr_evaluation_detail::entity(inputs, leaves.kill_entities[index])};
            if (id.is_valid()) {
                kills += inputs.ledger.kills(id);
            }
        }

        leaves_.results[leaf] = kills >= leaves.kill_targets[data];
    }
}
void ConditionManager::evaluate_reached_position(std::span<LeafIndex const> const run,
                                                 ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.history().life_state()};

    FrameArray<EntityId> ids{&inputs.scratch};
    FrameArray<LeafIndex> live_leaves{&inputs.scratch};
    auto const count{static_cast<std::uint32_t>(run.size())};
    ids.set_num(count);
    live_leaves.set_num(count);
    std::uint32_t live_count{};

    for (auto const leaf : run) {
        auto const data{leaves.data_handles[leaf].index()};
        auto const id{expr_evaluation_detail::entity(inputs, leaves.reached_entities[data])};
        if (!id.is_valid()) {
            continue;
        }

        if (!expr_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            ids[live_count] = id;
            live_leaves[live_count++] = leaf;
        }
    }

    Vectors3f positions{inputs.scratch};
    positions.add_uninitialised(live_count);
    auto const output{positions.view().resolve()};

    inputs.spatial.copy_entity_locations(
        inputs.lookups, std::span<EntityId const>{ids.data(), live_count}, output, &inputs.scratch);

    for (std::uint32_t row{}; row < live_count; ++row) {
        auto const leaf{live_leaves[row]};
        auto const data{leaves.data_handles[leaf].index()};
        auto const position{output[row]};
        auto const target{leaves.positions[data]};
        Vector3d const position_double{position.x(), position.y(), position.z()};

        if (size_squared(position_double - target) <= leaves.radius_squared[data]) {
            leaves_.results[leaf] = true;
        }
    }
}

/* **************************************** */
// Latches
/* **************************************** */
auto ConditionManager::add_latch(LatchInput const input) -> LatchHandle {
    assert(input.when.is_valid() && input.value.is_valid());
    assert(latch_states_.size() < LatchHandle::invalid_value);
    LatchHandle const handle{static_cast<std::uint32_t>(latch_states_.size())};

    latch_inputs_.push_back(input);
    latch_states_.emplace_back();
    active_latches_.push_back(handle);
    return handle;
}
void ConditionManager::evaluate_latches() {
    for (auto const handle : active_latches_) {
        auto const index{handle.index()};
        auto& state{latch_states_[index]};
        auto const input{latch_inputs_[index]};
        if (state.pending() == LatchState::Unset && result(input.when)) {
            auto const value{result(input.value)};
            state.set_pending(value ? LatchState::True : LatchState::False);
        }
    }
}
void ConditionManager::commit_latches() {
    for (auto remaining{active_latches_.size()}; remaining > 0; --remaining) {
        auto const index{remaining - 1};
        auto& state{latch_states_[active_latches_[index].index()]};
        auto const pending{state.pending()};
        if (pending != LatchState::Unset) {
            state.set_cached(pending);
            active_latches_[index] = active_latches_.back();
            active_latches_.pop_back();
        }
    }
}
auto ConditionManager::latch_state(LatchHandle const handle) const -> LatchState {
    assert(is_valid(handle));
    return latch_states_[handle.index()].cached();
}
auto ConditionManager::is_valid(LatchHandle const handle) const -> bool {
    return handle.is_valid() && handle.index() < latch_states_.size();
}

}
