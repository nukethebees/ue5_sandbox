#include <ioj/sim/expressions/expr_manager.h>

#include <ioj/frame_array.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/levels/expression.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/expressions/latches.h>
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
    assert(id.is_valid());
    auto const row{ledger.history_index(id)};
    assert(row < life_states.size());
    return life_states[row] != LifeState::Alive;
}
}

/* **************************************** */
// Compilation
/* **************************************** */
void ExprManager::append(std::span<levels::Expression const> const expressions,
                         ExprCompilationContext const& ctx,
                         std::span<ExprHandle> const references) {
    assert(references.size() == expressions.size());

    auto& leaves{leaves_};
    auto& composites{composites_};
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };
    auto const order{expr_compilation_detail::expression_order(expressions)};

    for (auto const index : order) {
        auto const& expression{expressions[index]};
        if (is_composite(expression.type())) {
            ExprCompositeIndex const composite{size(composites.results)};
            CompositeKind kind{};
            switch (expression.type()) {
                case levels::ExprType::All:
                    kind = CompositeKind::All;
                    break;
                case levels::ExprType::Any:
                    kind = CompositeKind::Any;
                    break;
                case levels::ExprType::Not:
                    kind = CompositeKind::Not;
                    break;
                case levels::ExprType::Xor:
                    kind = CompositeKind::Xor;
                    break;
                default:
                    std::unreachable();
            }

            references[index] = ExprHandle::make_data_composite(kind, composite);
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

        LeafKind kind{};
        LeafDataIndex data_index{};
        switch (expression.type()) {
            case levels::ExprType::EntityDestroyed: {
                auto const& condition{std::get<levels::EntityDestroyedExpr>(expression.payload)};

                kind = LeafKind::EntityDestroyed;
                data_index = static_cast<LeafDataIndex>(leaves.destroyed_entities.size());
                leaves.destroyed_entities.push_back(ctx.entities.at(condition.entity));
                break;
            }

            case levels::ExprType::EntityReachedPosition: {
                auto const& condition{
                    std::get<levels::EntityReachedPositionExpr>(expression.payload)};

                kind = LeafKind::EntityReachedPosition;
                data_index = static_cast<LeafDataIndex>(leaves.reached_entities.size());
                leaves.reached_entities.push_back(ctx.entities.at(condition.entity));
                leaves.positions.push_back(condition.position.get());
                leaves.radius_squared.push_back(condition.radius.get() * condition.radius.get());
                break;
            }

            case levels::ExprType::TimerElapsed: {
                kind = LeafKind::TimerElapsed;
                data_index = static_cast<LeafDataIndex>(leaves.timers.size());
                auto const timer_index{
                    std::get<levels::TimerElapsedExpr>(expression.payload).timer};
                auto const timer{ctx.timers[timer_index]};
                assert(timer.is_valid());
                leaves.timers.push_back(timer);
                break;
            }

            case levels::ExprType::LatchedTrue:
                kind = LeafKind::LatchedTrue;
                data_index = size(leaves.true_latches);
                leaves.true_latches.emplace_back(
                    std::get<levels::LatchedTrueExpr>(expression.payload).latch);
                break;
            case levels::ExprType::LatchedFalse:
                kind = LeafKind::LatchedFalse;
                data_index = size(leaves.false_latches);
                leaves.false_latches.emplace_back(
                    std::get<levels::LatchedFalseExpr>(expression.payload).latch);
                break;
            case levels::ExprType::EventFired:
                kind = LeafKind::EventFired;
                data_index = size(leaves.signals);
                leaves.signals.push_back(
                    ctx.signals.at(std::get<levels::EventFiredExpr>(expression.payload).signal));
                break;

            case levels::ExprType::EntityKillsReached: {
                auto const& condition{std::get<levels::EntityKillsReachedExpr>(expression.payload)};
                kind = LeafKind::EntityKillsReached;
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

        references[index] = ExprHandle::make_data_leaf(LeafHandle{leaf, kind});
        leaves.data_handles.emplace_back(data_index, kind);
        leaves.leaves_by_kind[kind].emplace_back(leaf, kind);
    }
}
auto ExprManager::add_timer_elapsed(TimerHandle const timer) -> ExprHandle {
    assert(timer.is_valid());
    auto const leaf{static_cast<LeafIndex>(leaves_.num_leaves())};
    auto const data{static_cast<LeafDataIndex>(leaves_.timers.size())};
    leaves_.timers.push_back(timer);
    leaves_.results.push_back(false);
    leaves_.data_handles.emplace_back(data, LeafKind::TimerElapsed);
    leaves_.leaves_by_kind[LeafKind::TimerElapsed].emplace_back(leaf, LeafKind::TimerElapsed);
    return ExprHandle::make_data_leaf(LeafHandle{leaf, LeafKind::TimerElapsed});
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void ExprManager::evaluate(ExprEvaluationContext const& inputs) {
    std::ranges::fill(leaves_.results, ExprResult{});
    evaluate_leaves(inputs);
    evaluate_composites();
}

/* **************************************** */
// Inspection
/* **************************************** */
auto ExprManager::result(ExprHandle const expression) const -> bool {
    assert(expression.is_valid());

    auto const leaf{expression.kind() == ExprKind::Leaf};
    auto const& results{leaf ? leaves_.results : composites_.results};
    auto const index{leaf ? expression.data_leaf_index() : expression.data_composite_index()};
    assert(index < results.size());
    return results[index];
}

/* **************************************** */
// Composite evaluation
/* **************************************** */
void ExprManager::evaluate_composites() {
    auto const count{composites_.results.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const kind{composites_.kinds[index]};
        auto const range{composites_.child_ranges[index]};
        auto const children{std::span{composites_.children}.subspan(range.offset, range.count)};
        bool value{kind == CompositeKind::All};
        if (kind == CompositeKind::Not) {
            value = !result(children.front());
        } else {
            for (auto const child : children) {
                auto const operand{result(child)};
                switch (kind) {
                    case CompositeKind::All:
                        value &= operand;
                        break;
                    case CompositeKind::Any:
                        value |= operand;
                        break;
                    case CompositeKind::Xor:
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
void ExprManager::evaluate_leaves(ExprEvaluationContext const& inputs) {
    evaluate_destroyed(leaves_.leaves_by_kind[LeafKind::EntityDestroyed], inputs);
    evaluate_elapsed(leaves_.leaves_by_kind[LeafKind::TimerElapsed], inputs);
    evaluate_kills(leaves_.leaves_by_kind[LeafKind::EntityKillsReached], inputs);

    auto const& positions{leaves_.leaves_by_kind[LeafKind::EntityReachedPosition]};
    if (!positions.empty()) {
        evaluate_reached_position(positions, inputs);
    }

    for (auto const handle : leaves_.leaves_by_kind[LeafKind::LatchedTrue]) {
        auto const index{handle.index()};
        leaves_.results[index] =
            inputs.latches.latched_true(leaves_.true_latches[leaves_.data_handles[index].index()]);
    }

    for (auto const handle : leaves_.leaves_by_kind[LeafKind::LatchedFalse]) {
        auto const index{handle.index()};
        leaves_.results[index] = inputs.latches.latched_false(
            leaves_.false_latches[leaves_.data_handles[index].index()]);
    }

    for (auto const handle : leaves_.leaves_by_kind[LeafKind::EventFired]) {
        auto const index{handle.index()};
        leaves_.results[index] =
            inputs.signals[leaves_.signals[leaves_.data_handles[index].index()].index()];
    }
}
void ExprManager::evaluate_destroyed(std::span<LeafHandle const> const run,
                                     ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.unique_entities().life_state()};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
        auto const data{leaves.data_handles[leaf].index()};
        auto const id{expr_evaluation_detail::entity(inputs, leaves.destroyed_entities[data])};

        if (id.is_valid() && expr_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            leaves_.results[leaf] = true;
        }
    }
}
void ExprManager::evaluate_elapsed(std::span<LeafHandle const> const run,
                                   ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
        auto const data{leaves.data_handles[leaf].index()};

        if (inputs.timers.elapsed(leaves.timers[data])) {
            leaves_.results[leaf] = true;
        }
    }
}
void ExprManager::evaluate_kills(std::span<LeafHandle const> const run,
                                 ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
        auto const data{leaves.data_handles[leaf].index()};
        std::uint64_t kills{};
        auto const sources{leaves.kill_sources[data]};
        auto const sources_end{sources.end()};
        for (auto index{sources.start()}; index < sources_end; ++index) {
            auto const id{expr_evaluation_detail::entity(inputs, leaves.kill_entities[index])};
            if (id.is_valid()) {
                kills += inputs.ledger.kills(id);
            }
        }

        if (kills >= leaves.kill_targets[data]) {
            leaves_.results[leaf] = true;
        }
    }
}
void ExprManager::evaluate_reached_position(std::span<LeafHandle const> const run,
                                            ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.unique_entities().life_state()};

    assert(inputs.spatial && inputs.scratch);

    FrameArray<EntityId> ids{inputs.scratch};
    FrameArray<LeafIndex> live_leaves{inputs.scratch};
    auto const count{static_cast<std::uint32_t>(run.size())};
    ids.set_num(count);
    live_leaves.set_num(count);
    std::uint32_t live_count{};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
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

    Vectors3f positions{*inputs.scratch};
    positions.add_uninitialised(live_count);
    auto const output{positions.view().resolve()};

    inputs.spatial->copy_entity_locations(
        std::span<EntityId const>{ids.data(), live_count}, output, inputs.scratch);

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

}
