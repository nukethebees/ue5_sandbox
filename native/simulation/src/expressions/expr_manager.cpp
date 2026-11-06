#include <ioj/sim/expressions/expr_manager.h>

#include <ioj/container_ops.h>
#include <ioj/frame_array.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace ioj::sim {
namespace expr_compilation_detail {
struct ExpressionVisit {
    levels::ExprIndex node;
    std::size_t next_child{};
};

auto expression_order(std::span<levels::MissionExpr const> const expressions)
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
auto entity(ExprEvaluationContext const& inputs, LevelEntityIndex const slot) -> EntityId {
    assert(slot < inputs.entities.size());
    return inputs.entities[slot];
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
auto ExprManager::append(std::span<levels::MissionExpr const> const expressions,
                         ExprCompilationContext const& ctx)
    -> std::expected<Compilation, std::vector<std::string>> {
    auto& leaves{leaves_};
    auto& composites{composites_};
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };
    Compilation compiled;
    compiled.leaves.offset = static_cast<LeafIndex>(leaves.num_leaves());
    compiled.composites.offset = size(composites.results);
    compiled.handles.resize(expressions.size());
    auto& references{compiled.handles};
    auto const order{expr_compilation_detail::expression_order(expressions)};
    for (auto const index : order) {
        auto const& expression{expressions[index]};
        if (is_composite(expression.type())) {
            ExprCompositeIndex const composite{size(composites.results)};
            auto const kind{expression.type() == levels::ExprType::All ? CompositeKind::All
                                                                       : CompositeKind::Any};

            references[index] = ExprHandle::make_data_composite(kind, composite);
            composites.results.push_back(ExprResult::Pending);
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
        leaves.results.push_back(ExprResult::Pending);

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

            case levels::ExprType::TimeElapsed: {
                auto const& condition{std::get<levels::TimeElapsedExpr>(expression.payload)};
                kind = LeafKind::TimeElapsed;
                data_index = static_cast<LeafDataIndex>(leaves.durations.size());

                auto const ticks{condition.seconds.get() * ctx.clock.tick_rate().hz()};
                if (ticks >= static_cast<double>(std::numeric_limits<SimTick::value_type>::max())) {
                    return std::unexpected{
                        std::vector<std::string>{"Expression timer exceeds tick range"}};
                }

                leaves.durations.emplace_back(
                    ctx.clock.duration_to_tick_period(Duration::make(condition.seconds.get()))
                        .get());
                leaves.activated_at.emplace_back();
                break;
            }

            case levels::ExprType::ObjectiveResolved: {
                auto const& condition{std::get<levels::ObjectiveResolvedExpr>(expression.payload)};
                kind = LeafKind::ObjectiveResolved;
                data_index = static_cast<LeafDataIndex>(leaves.observed_objectives.size());
                auto const observed{ctx.objectives.at(condition.objective.objective)};

                leaves.observed_objectives.push_back(observed);
                leaves.observed_outcomes.push_back(condition.objective.outcome);
                break;
            }

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
    }

    compiled.leaves.count = static_cast<LeafIndex>(leaves.num_leaves()) - compiled.leaves.offset;
    compiled.composites.count = size(composites.results) - compiled.composites.offset;
    return compiled;
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void ExprManager::reset_progress() {
    leaves_.reset_progress();
    composites_.reset_progress();
}
void ExprManager::activate(IndexSpan const leaves, IndexSpan const composites, SimTick const tick) {
    auto const leaves_end{leaves.end()};
    for (LeafIndex leaf{leaves.start()}; leaf < leaves_end; ++leaf) {
        auto const data_handle{leaves_.data_handles[leaf]};
        if (data_handle.kind() == LeafKind::TimeElapsed) {
            leaves_.activated_at[data_handle.index()] = tick;
        }
        leaves_.active.emplace_back(leaf, data_handle.kind());
    }

    auto const composites_end{composites.end()};
    for (ExprCompositeIndex index{composites.start()}; index < composites_end; ++index) {
        composites_.active.push_back(index);
    }
}
void ExprManager::deactivate(IndexSpan const leaves, IndexSpan const composites) {
    remove_at_swap_if(leaves_.active, [&](LeafHandle const handle) {
        auto const index{handle.index()};
        return index >= leaves.start() && index < leaves.end();
    });
    std::erase_if(composites_.active, [&](ExprCompositeIndex const index) {
        return index >= composites.start() && index < composites.end();
    });
}
void ExprManager::evaluate(ExprEvaluationContext const& inputs, Mission const& mission) {
    std::ranges::sort(leaves_.active);
    evaluate_leaves(inputs, mission);
    evaluate_composites();
}

/* **************************************** */
// Inspection
/* **************************************** */
auto ExprManager::result(ExprHandle const expression) const -> ExprResult {
    if (!expression.is_valid()) {
        return ExprResult::False;
    }

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
    for (auto const index : composites_.active) {
        auto const all{composites_.kinds[index] == CompositeKind::All};
        auto const decisive{all ? ExprResult::False : ExprResult::True};
        auto value{all ? ExprResult::True : ExprResult::False};

        auto const children{composites_.child_ranges[index]};
        auto const children_end{children.end()};

        for (auto child{children.start()}; child < children_end; ++child) {
            auto const child_result{result(composites_.children[child])};
            if (child_result == decisive) {
                value = decisive;
                break;
            }

            if (child_result == ExprResult::Pending) {
                value = ExprResult::Pending;
            }
        }

        composites_.results[index] = value;
    }

    std::erase_if(composites_.active, [&](ExprCompositeIndex const index) {
        return composites_.results[index] != ExprResult::Pending;
    });
}

/* **************************************** */
// Leaf evaluation
/* **************************************** */
void ExprManager::resolve_leaf(LeafIndex const index, ExprResult const value) {
    assert(value != ExprResult::Pending);
    leaves_.results[index] = value;
}
void ExprManager::evaluate_leaves(ExprEvaluationContext const& inputs, Mission const& mission) {
    auto const scheduled{std::span<LeafHandle const>{leaves_.active}};
    assert(std::ranges::is_sorted(scheduled));

    auto const count{scheduled.size()};
    std::size_t first{};

    while (first < count) {
        auto const kind{scheduled[first].kind()};
        auto end{first + 1};
        while (end < count && scheduled[end].kind() == kind) {
            ++end;
        }

        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select the current leaf kind's range.
        auto const run{scheduled.subspan(first, end - first)};
        switch (kind) {
            case LeafKind::EntityDestroyed:
                evaluate_destroyed(run, inputs);
                break;
            case LeafKind::TimeElapsed:
                evaluate_elapsed(run, inputs);
                break;
            case LeafKind::ObjectiveResolved:
                evaluate_resolved(run, mission);
                break;
            case LeafKind::EntityKillsReached:
                evaluate_kills(run, inputs);
                break;
            case LeafKind::EntityReachedPosition:
                evaluate_reached_position(run, inputs);
                break;
        }
        first = end;
    }

    // Stop evaluating resolved leaves on later ticks; retain their results.
    remove_at_swap_if(leaves_.active, [&](LeafHandle const handle) {
        return leaves_.results[handle.index()] != ExprResult::Pending;
    });
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
            resolve_leaf(leaf, ExprResult::True);
        }
    }
}
void ExprManager::evaluate_elapsed(std::span<LeafHandle const> const run,
                                   ExprEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
        auto const data{leaves.data_handles[leaf].index()};
        auto const activated{leaves.activated_at[data]};
        assert(inputs.tick >= activated);

        if (inputs.tick.value() - activated.value() >= leaves.durations[data].value()) {
            resolve_leaf(leaf, ExprResult::True);
        }
    }
}
void ExprManager::evaluate_resolved(std::span<LeafHandle const> const run, Mission const& mission) {
    auto const& leaves{leaves_};

    for (auto const handle : run) {
        auto const leaf{handle.index()};
        auto const data{leaves.data_handles[leaf].index()};
        auto const observed_outcome{mission.outcome(leaves.observed_objectives[data], *this)};
        if (observed_outcome) {
            resolve_leaf(leaf,
                         *observed_outcome == leaves.observed_outcomes[data] ? ExprResult::True
                                                                             : ExprResult::False);
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
            resolve_leaf(leaf, ExprResult::True);
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

        if (expr_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            resolve_leaf(leaf, ExprResult::False);
        } else {
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
            resolve_leaf(leaf, ExprResult::True);
        }
    }
}

}
