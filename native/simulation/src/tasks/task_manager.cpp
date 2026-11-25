#include <ioj/sim/tasks/task_manager.h>

#include <ioj/enum_array.h>
#include <ioj/enum_traits.h>
#include <ioj/require.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/expressions/condition_manager.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/tasks/executor_table.h>
#include <ioj/sim/tasks/task_allocation_context.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace ioj::sim::tasks {
namespace task_manager_detail {
template <typename... Enums, typename Factory>
auto make_array(Factory&& factory) {
    return [&factory]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        using Value = decltype(factory());
        return EnumArray<Value, Enums...>{
            std::array<Value, sizeof...(Indices)>{(static_cast<void>(Indices), factory())...}};
    }(std::make_index_sequence<(enum_count<Enums>() * ...)>{});
}
}

[[nodiscard]] constexpr auto fits_instruction_range(std::size_t const first,
                                                    std::size_t const count) noexcept -> bool {
    return first < InstructionHandle::invalid_value &&
           count <= InstructionHandle::invalid_value - first;
}

[[nodiscard]] constexpr auto is_valid_task_index(std::size_t const index) noexcept -> bool {
    return index < TaskId::invalid_value;
}

/* **************************************** */
// Lifecycle
/* **************************************** */
TaskManager::TaskManager(std::pmr::memory_resource& resource)
    : instructions_{&resource}
    , instruction_data_{resource}
    , task_entries_{&resource}
    , task_values_{&resource}
    , active_tasks_{task_manager_detail::make_array<Team>(
          [&resource] { return std::pmr::vector<TaskId>{&resource}; })}
    , task_entity_allocations_{task_manager_detail::make_array<Team, EntityType>(
          [&resource] { return Counts{&resource}; })}
    , free_entities_{task_manager_detail::make_array<Team, EntityType>(
          [&resource] { return std::pmr::vector<EntityId>{&resource}; })}
    , checked_trigger_ranges_{&resource} {
    reset();
}
void TaskManager::reset() {
    allocation_countdown_ = initial_allocation_countdown;
    instructions_.clear();
    instruction_data_.locations.clear();
    task_entries_.clear();
    task_values_.clear();
    for (auto& tasks : active_tasks_) {
        tasks.clear();
    }
    for (auto& counts : task_entity_allocations_) {
        counts.clear();
    }
    for (auto& entities : free_entities_) {
        entities.clear();
    }
    checked_trigger_ranges_.clear();

    static_cast<void>(add_task({Instruction::make_data_idle(), jump_to_idle}));
}
/* **************************************** */
// Task registration
/* **************************************** */
void TaskManager::reserve_instructions(std::size_t const count) {
    assert(fits_instruction_range(0, count));
    instructions_.reserve(count);
}
auto TaskManager::add_task(std::span<Instruction const> const instructions) -> TaskId {
    require(!instructions.empty() && instructions.back() == jump_to_idle,
            "Task instructions must end with Jump 0");

    auto const first{instructions_.size()};
    assert(fits_instruction_range(first, instructions.size()));

    auto const index{task_entries_.size()};
    assert(is_valid_task_index(index));

    instructions_.insert(instructions_.end(), instructions.begin(), instructions.end());
    task_entries_.push_back(InstructionHandle{static_cast<InstructionHandle::storage_type>(first)});
    task_values_.push_back(0);
    for (auto& counts : task_entity_allocations_) {
        counts.push_back(0);
    }
    return TaskId{static_cast<TaskId::storage_type>(index)};
}
auto TaskManager::add_task(std::initializer_list<Instruction> const instructions) -> TaskId {
    return add_task(std::span<Instruction const>{instructions.begin(), instructions.size()});
}

/* **************************************** */
// Entity registration
/* **************************************** */
void TaskManager::register_entities(Team const team, std::span<EntityId const> const ids) {
    for (auto const id : ids) {
        free_entities_[team, id.entity_type()].push_back(id);
    }
}
void TaskManager::release_assignment(Team const team, EntityId const entity, TaskId const task) {
    auto& count{task_entity_allocations_[team, entity.entity_type()][task.index()]};
    assert(task != idle_task && count > 0);
    --count;
}
void TaskManager::finish_task(Team const team, EntityId const entity, TaskId const task) {
    release_assignment(team, entity, task);
    free_entities_[team, entity.entity_type()].push_back(entity);
}
void TaskManager::unregister_dead_entities(std::span<EntityId const> const victims,
                                           ExecutorConstView const executors,
                                           EntityLedger const& ledger) {
    if (victims.empty()) {
        return;
    }

    auto const type{executors.entity_type};
    auto const offset{entity_identity_offsets[type]};
    auto const issued{ledger.issued_counts()[type]};
    auto const history{ledger.history()};
    auto const teams{history.teams().subspan(offset, issued)};
    auto const life_states{history.life_state().subspan(offset, issued)};

    for (auto const id : victims) {
        auto const index{id.index()};
        assert(id.entity_type() == type && index < issued &&
               !executors.pending_tasks[index].is_valid());
        auto const task{executors.current_tasks[index]};
        if (task != idle_task) {
            release_assignment(teams[index], id, task);
        }
    }

    for (auto const team : EnumTraits<Team>::values) {
        auto& free{free_entities_[team, type]};
        std::erase_if(free, [life_states](EntityId const id) {
            return life_states[id.index()] != LifeState::Alive;
        });
    }
}

/* **************************************** */
// Planning and allocation
/* **************************************** */
namespace task_manager_detail {
void collect_live_targets(ExprHandle const condition,
                          TaskAllocationContext const ctx,
                          std::pmr::vector<ExprHandle>& pending_exprs,
                          std::pmr::vector<ExprHandle>& visited_exprs,
                          std::pmr::vector<EntityId>& targets) {
    auto const& ledger{ctx.level.entity_ledger()};
    auto const life_states{ledger.history().life_state()};
    pending_exprs.clear();
    visited_exprs.clear();
    targets.clear();
    pending_exprs.push_back(condition);

    auto const already_visited{[&visited_exprs](ExprHandle const expression) {
        for (auto const visited : visited_exprs) {
            if (visited == expression) {
                return true;
            }
        }
        return false;
    }};

    while (!pending_exprs.empty()) {
        auto const expression{pending_exprs.back()};
        pending_exprs.pop_back();
        if (already_visited(expression)) {
            continue;
        }

        visited_exprs.push_back(expression);
        auto const expr_kind{expression.kind()};
        switch (expr_kind) {
            case ExprKind::All:
            case ExprKind::Any: {
                // Each popped expression selects its own children.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                auto const children{ctx.conditions.children(expression)};
                pending_exprs.insert(pending_exprs.end(), children.rbegin(), children.rend());
                break;
            }
            case ExprKind::LatchedTrue:
                pending_exprs.push_back(ctx.conditions.latched_true_value(expression));
                break;
            case ExprKind::EntityDestroyed: {
                auto const entity{ctx.conditions.destroyed_entity(expression)};
                auto const target{ctx.entities[entity.index()]};
                if (!target.is_valid()) {
                    break;
                }

                auto const row{ledger.history_index(target)};
                if (life_states[row] != LifeState::Alive) {
                    break;
                }

                bool already_collected{};
                for (auto const collected : targets) {
                    if (collected == target) {
                        already_collected = true;
                        break;
                    }
                }
                if (!already_collected) {
                    targets.push_back(target);
                }
                break;
            }
            default:
                break;
        }
    }
}
}

void TaskManager::allocate(TaskAllocationContext const ctx) {
    --allocation_countdown_;
    if (allocation_countdown_ != 0) {
        return;
    }

    allocation_countdown_ = allocation_period_ticks;

    auto* const scratch{&ctx.scratch_resource};

    // Sum team contributions by condition.
    struct RootTaskValues {
        ExprHandle condition;
        TeamArray<TaskValue> task_values{};
    };

    std::pmr::vector<RootTaskValues> roots{scratch};

    auto const active_triggers{ctx.events.active_triggers()};
    auto const triggers{ctx.events.triggers()};
    auto const trigger_events{ctx.events.trigger_events()};
    for (auto const index : active_triggers) {
        if (is_trigger_checked(index)) {
            continue;
        }

        auto const trigger{triggers[index]};
        std::size_t const begin{trigger.events.offset};
        auto const end{begin + trigger.events.count};
        for (auto event_index{begin}; event_index < end; ++event_index) {
            auto const trigger_event{trigger_events[event_index]};
            auto const event_kind{trigger_event.kind()};

            if (is_task_value_event(event_kind)) {
                RootTaskValues* root{};
                for (auto& candidate : roots) {
                    if (candidate.condition == trigger.condition) {
                        root = &candidate;
                        break;
                    }
                }
                if (root == nullptr) {
                    root = &roots.emplace_back(trigger.condition);
                }

                if (event_kind == EventKind::Win) {
                    constexpr TaskValue win_value{1000};
                    root->task_values[ctx.events.win_event(trigger_event).winner] += win_value;
                } else {
                    auto const change{ctx.events.change_points_event(trigger_event)};
                    root->task_values[change.team] += change.delta;
                }
            }

            if (is_terminal(event_kind)) {
                break;
            }
        }

        // Discovery is one-shot, including targets that have not spawned yet.
        mark_trigger_checked(index);
    }

    // Merge contributions by team and target.
    std::pmr::vector<ExprHandle> pending_exprs{scratch};
    std::pmr::vector<ExprHandle> visited_exprs{scratch};
    std::pmr::vector<EntityId> targets{scratch};
    if (!roots.empty()) {
        constexpr std::size_t initial_capacity{8};
        pending_exprs.reserve(initial_capacity);
        visited_exprs.reserve(initial_capacity);
        targets.reserve(initial_capacity);
    }

    for (auto const& root : roots) {
        task_manager_detail::collect_live_targets(
            root.condition, ctx, pending_exprs, visited_exprs, targets);
        // Apply each team's nonzero contribution once per target.
        for (auto const target : targets) {
            for (auto const team : EnumTraits<Team>::values) {
                auto const contribution{root.task_values[team]};
                if (contribution == 0) {
                    continue;
                }

                auto& team_tasks{active_tasks_[team]};
                TaskId matching_task{};
                for (auto const candidate_task : team_tasks) {
                    if (instruction(first_instruction(candidate_task)).data_kill() == target) {
                        matching_task = candidate_task;
                        break;
                    }
                }

                if (!matching_task.is_valid()) {
                    matching_task = add_task({Instruction::make_data_kill(target), jump_to_idle});
                    team_tasks.push_back(matching_task);
                }
                task_values_[matching_task.index()] += contribution;
            }
        }
    }

    // Assign free entities by team.
    auto const& ledger{ctx.level.entity_ledger()};
    auto const life_states{ledger.history().life_state()};
    for (auto const team : EnumTraits<Team>::values) {
        auto& team_tasks{active_tasks_[team]};

        // Retire tasks whose targets died since discovery.
        std::erase_if(team_tasks, [&](TaskId const task) {
            auto const target{instruction(first_instruction(task)).data_kill()};
            return life_states[ledger.history_index(target)] != LifeState::Alive;
        });

        if (team_tasks.empty()) {
            continue;
        }

        // Assign each entity type independently in round-robin order.
        for (auto const entity_type : EnumTraits<EntityType>::values) {
            auto& free_entities{free_entities_[team, entity_type]};
            if (free_entities.empty()) {
                continue;
            }

            auto const pending_tasks{ctx.executors.view(entity_type).pending_tasks};
            auto& assigned_entities_per_task{task_entity_allocations_[team, entity_type]};

            auto const free_entity_count{free_entities.size()};
            std::size_t entities_assigned{};
            while (entities_assigned < free_entity_count) {
                auto const entities_assigned_before_lap{entities_assigned};
                for (auto const task : team_tasks) {
                    if (task_value(task) <= 0) {
                        continue;
                    }

                    auto const entity{free_entities[entities_assigned]};
                    pending_tasks[entity.index()] = task;
                    ++assigned_entities_per_task[task.index()];
                    ++entities_assigned;
                    if (entities_assigned == free_entity_count) {
                        break;
                    }
                }

                if (entities_assigned == entities_assigned_before_lap) {
                    break;
                }
            }

            if (entities_assigned == free_entity_count) {
                free_entities.clear();
            }
        }
    }
}
auto TaskManager::is_trigger_checked(std::uint32_t const index) const -> bool {
    for (auto const range : checked_trigger_ranges_) {
        if (index < range.start) {
            return false;
        }
        if (index < range.end) {
            return true;
        }
    }
    return false;
}
void TaskManager::mark_trigger_checked(std::uint32_t const index) {
    TriggerIndexRange range{index, index + 1};
    assert(range.start < range.end);

    auto const end{checked_trigger_ranges_.end()};
    auto first{checked_trigger_ranges_.begin()};
    while (first != end && first->end < range.start) {
        ++first;
    }

    auto last{first};
    while (last != end && last->start <= range.end) {
        range.start = std::min(range.start, last->start);
        range.end = std::max(range.end, last->end);
        ++last;
    }

    if (first == last) {
        checked_trigger_ranges_.insert(first, range);
    } else {
        *first = range;
        checked_trigger_ranges_.erase(first + 1, last);
    }
}
}
