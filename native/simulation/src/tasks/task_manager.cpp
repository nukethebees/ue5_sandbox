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
#include <unordered_set>
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
    , active_tasks_{task_manager_detail::make_array<Team>(
          [&resource] { return std::pmr::vector<TaskId>{&resource}; })}
    , task_entity_allocations_{task_manager_detail::make_array<Team, EntityType>(
          [&resource] { return Counts{&resource}; })}
    , free_entities_{task_manager_detail::make_array<Team, EntityType>(
          [&resource] { return std::pmr::vector<EntityId>{&resource}; })}
    , checked_trigger_runs_{&resource} {
    reset();
}
void TaskManager::reset() {
    allocation_countdown_ = initial_allocation_countdown;
    instructions_.clear();
    instruction_data_.locations.clear();
    task_entries_.clear();
    for (auto& tasks : active_tasks_) {
        tasks.clear();
    }
    for (auto& counts : task_entity_allocations_) {
        counts.clear();
    }
    for (auto& entities : free_entities_) {
        entities.clear();
    }
    checked_trigger_runs_.clear();

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
void TaskManager::remove_entities(Team const team, std::span<EntityId const> const ids) {
    for (auto const id : ids) {
        std::erase(free_entities_[team, id.entity_type()], id);
    }
}

/* **************************************** */
// Planning and allocation
/* **************************************** */
void TaskManager::allocate(TaskAllocationContext const ctx) {
    --allocation_countdown_;
    if (allocation_countdown_ != 0) {
        return;
    }

    allocation_countdown_ = allocation_period_ticks;

    auto const active_triggers{ctx.events.active_triggers()};
    auto const triggers{ctx.events.triggers()};
    for (auto const index : active_triggers) {
        if (is_trigger_checked(index)) {
            continue;
        }

        auto const trigger{triggers[index]};
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- each trigger owns a different event range.
        for (auto const event : ctx.events.trigger_events(index)) {
            auto const kind{event.kind()};
            switch (kind) {
                case EventKind::Win:
                    add_kill_tasks(trigger.condition, ctx.events.win_event(event).winner, ctx);
                    goto trigger_checked;
                case EventKind::StopRun:
                    goto trigger_checked;
                default:
                    break;
            }
        }

    trigger_checked:
        // Discovery is one-shot, including targets that have not spawned yet.
        add_checked_trigger_run({index, index + 1});
    }

    allocate_free_entities(ctx);
}
void TaskManager::add_kill_tasks(ExprHandle const condition,
                                 Team const team,
                                 TaskAllocationContext const ctx) {
    auto const& ledger{ctx.level.entity_ledger()};
    auto const life_states{ledger.history().life_state()};
    std::vector<ExprHandle> pending{condition};
    std::unordered_set<ExprHandle> visited;
    std::unordered_set<EntityId> targets;

    while (!pending.empty()) {
        auto const expression{pending.back()};
        pending.pop_back();
        if (visited.contains(expression)) {
            continue;
        }

        visited.insert(expression);
        auto const kind{expression.kind()};
        switch (kind) {
            case ExprKind::All:
            case ExprKind::Any: {
                // Each popped expression selects its own children.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                auto const children{ctx.conditions.children(expression)};
                pending.insert(pending.end(), children.rbegin(), children.rend());
                break;
            }
            case ExprKind::LatchedTrue:
                pending.push_back(ctx.conditions.latched_true_value(expression));
                break;
            case ExprKind::EntityDestroyed: {
                auto const entity{ctx.conditions.destroyed_entity(expression)};
                auto const target{ctx.entities[entity.index()]};
                if (!target.is_valid() || targets.contains(target)) {
                    break;
                }

                auto const row{ledger.history_index(target)};
                if (life_states[row] != LifeState::Alive) {
                    break;
                }

                targets.insert(target);
                auto const task{add_task({Instruction::make_data_kill(target), jump_to_idle})};
                active_tasks_[team].push_back(task);
                break;
            }
            default:
                break;
        }
    }
}
void TaskManager::allocate_free_entities(TaskAllocationContext const ctx) {
    auto const& ledger{ctx.level.entity_ledger()};
    auto const life_states{ledger.history().life_state()};
    for (auto const team : EnumTraits<Team>::values) {
        auto& tasks{active_tasks_[team]};
        std::erase_if(tasks, [&](TaskId const task) {
            auto const target{instruction(first_instruction(task)).data_kill()};
            return life_states[ledger.history_index(target)] != LifeState::Alive;
        });
        if (tasks.empty()) {
            continue;
        }

        auto const task_count{tasks.size()};
        for (auto const type : EnumTraits<EntityType>::values) {
            auto& entities{free_entities_[team, type]};
            if (entities.empty()) {
                continue;
            }

            auto const pending_tasks{ctx.executors.view(type).pending_tasks};
            auto& counts{task_entity_allocations_[team, type]};

            // Round-robin assignment
            std::size_t task_index{};
            for (auto const entity : entities) {
                auto const task{tasks[task_index]};
                pending_tasks[entity.index()] = task;
                ++counts[task.index()];

                ++task_index;
                if (task_index == task_count) {
                    task_index = 0;
                }
            }

            entities.clear();
        }
    }
}
auto TaskManager::is_trigger_checked(std::uint32_t const index) const -> bool {
    for (auto const run : checked_trigger_runs_) {
        if (index < run.start) {
            return false;
        }
        if (index < run.end) {
            return true;
        }
    }
    return false;
}
void TaskManager::add_checked_trigger_run(TriggerRun run) {
    assert(run.start < run.end);

    auto const end{checked_trigger_runs_.end()};
    auto first{checked_trigger_runs_.begin()};
    while (first != end && first->end < run.start) {
        ++first;
    }

    auto last{first};
    while (last != end && last->start <= run.end) {
        run.start = std::min(run.start, last->start);
        run.end = std::max(run.end, last->end);
        ++last;
    }

    if (first == last) {
        checked_trigger_runs_.insert(first, run);
    } else {
        *first = run;
        checked_trigger_runs_.erase(first + 1, last);
    }
}
}
