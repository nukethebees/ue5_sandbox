#include <ioj/sim/tasks/task_manager.h>

#include <ioj/enum_array.h>
#include <ioj/require.h>
#include <ioj/sim/missions/mission.h>

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

TaskManager::TaskManager(std::pmr::memory_resource& resource)
    : instructions_{&resource}
    , instruction_data_{resource}
    , task_entries_{&resource}
    , task_actionable_pass_leaves_{&resource}
    , task_leaf_teams_{&resource}
    , leaf_tasks_{&resource}
    , active_tasks_{task_manager_detail::make_array<Team>(
          [&resource] { return std::pmr::vector<TaskId>{&resource}; })}
    , task_entity_allocations_{task_manager_detail::make_array<Team, EntityType>(
          [&resource] { return Counts{&resource}; })} {
    reset();
}

void TaskManager::reset() {
    instructions_.clear();
    instruction_data_.locations.clear();
    task_entries_.clear();
    task_actionable_pass_leaves_.clear();
    task_leaf_teams_.clear();
    leaf_tasks_.clear();
    for (auto& tasks : active_tasks_) {
        tasks.clear();
    }
    for (auto& counts : task_entity_allocations_) {
        counts.clear();
    }

    static_cast<void>(add_task({Instruction::make_data_idle(), jump_to_idle}));
}

void TaskManager::initialise(Mission const& mission) {
    reset();

    auto const leaf_count{mission.leaves().num_leaves()};
    leaf_tasks_.assign(leaf_count, TaskId{});
    task_actionable_pass_leaves_.assign(leaf_count, std::uint8_t{0});
    task_leaf_teams_.assign(leaf_count, Team{});

    auto const& objectives{mission.objectives()};
    auto const objective_count{objectives.size()};
    for (ObjectiveId::storage_type index{}; index < objective_count; ++index) {
        auto const objective{ObjectiveId{index}};
        auto const root{objectives.roots(objective)[ObjectiveOutcome::Passed]};
        auto const team{objectives.team(objective)};
        mission.visit_expression_tree(root, [this, team](ExprHandle const expression) {
            if (expression.kind() == ExprKind::Leaf && is_task(expression.data_leaf_kind())) {
                auto const leaf_index{expression.data_leaf_index()};
                task_actionable_pass_leaves_[leaf_index] = 1;
                task_leaf_teams_[leaf_index] = team;
            }
        });
    }
}

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

void TaskManager::plan_tasks(TaskPlanningContext const ctx) {
    auto const& leaves{ctx.mission.leaves()};
    assert(leaf_tasks_.size() == leaves.num_leaves());

    for (auto const leaf : leaves.active) {
        auto const index{leaf.index()};
        if (!task_actionable_pass_leaves_[index]) {
            continue;
        }

        auto& task{leaf_tasks_[index]};
        if (!task.is_valid()) {
            task = create_task(leaf, leaves, ctx.entities);
        }
    }
}

void TaskManager::allocate(TaskAllocationContext) {}

auto TaskManager::create_task(LeafHandle const leaf,
                              ExprLeaves const& leaves,
                              std::span<EntityId const> const entities) -> TaskId {
    auto const i{leaf.index()};

    switch (leaf.kind()) {
        case LeafKind::EntityDestroyed: {
            auto const data{leaves.data_handles[i].index()};
            auto const entity_index{leaves.destroyed_entities[data]};
            auto const entity_id{entities[entity_index]};
            if (!entity_id.is_valid()) {
                // Leave the task invalid so planning retries after the target spawns.
                return {};
            }

            return add_task({Instruction::make_data_kill(entity_id), jump_to_idle});
        }
        case LeafKind::EntityReachedPosition: {
            return {};
        }
        case LeafKind::EntityKillsReached: {
            return {};
        }
        default: {
            assert(false && "Leaf kind does not support tasks");
            return {};
        }
    }
}
}
