#include <ioj/sim/tasks/task_manager.h>

#include <ioj/enum_array.h>
#include <ioj/require.h>
#include <ioj/sim/levels/level_event_manager.h>
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
    for (auto const index : active_triggers) {
        if (is_trigger_checked(index)) {
            continue;
        }

        // TODO: Analyse ctx.events.triggers()[index] for potential objectives.
        add_checked_trigger_run({index, index + 1});
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
