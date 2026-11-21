#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/team_entity_array.h>
#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_entity_count.h>
#include <ioj/sim/tasks/task_id.h>
#include <ioj/team_array.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {
class EntityLedger;
}

namespace ioj::sim::tasks {
struct ExecutorConstView;
struct TaskAllocationContext;

// Store task programs. Allocate them to entities.
class TaskManager {
    struct TriggerRun {
        std::uint32_t start;
        std::uint32_t end;
    };

  public:
    using Counts = std::pmr::vector<TaskEntityCount>;
    using AllocationCountdown = std::uint32_t;

    static constexpr TaskId idle_task{0};
    static constexpr InstructionHandle idle_instruction{0};
    static constexpr Instruction jump_to_idle{Instruction::make_data_jump(idle_instruction)};
    static constexpr AllocationCountdown allocation_period_ticks{60};
    static constexpr AllocationCountdown initial_allocation_countdown{1};

    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    // Keep the resource alive until after this manager and its storage are destroyed.
    explicit TaskManager(std::pmr::memory_resource& resource = *std::pmr::new_delete_resource());

    // Clears free entities and checked triggers, invalidating custom task/instruction handles.
    // Retains allocated storage.
    void reset();

    /* **************************************** */
    // Task registration
    /* **************************************** */
    void reserve_instructions(std::size_t count);
    // Task instructions must end with jump_to_idle. Registration is independent of team activation.
    [[nodiscard]] auto add_task(std::span<Instruction const> instructions) -> TaskId;
    [[nodiscard]] auto add_task(std::initializer_list<Instruction> instructions) -> TaskId;

    /* **************************************** */
    // Entity registration
    /* **************************************** */
    void register_entities(Team team, std::span<EntityId const> ids);
    void finish_task(Team team, EntityId entity, TaskId task);
    // Publish the whole death batch to the ledger before unregistering its executors.
    void unregister_dead_entities(std::span<EntityId const> victims,
                                  ExecutorConstView executors,
                                  EntityLedger const& ledger);

    /* **************************************** */
    // Accessors
    /* **************************************** */
    [[nodiscard]] auto instruction(InstructionHandle const handle) const -> Instruction {
        assert(handle.index() < instructions_.size());
        return instructions_[handle.index()];
    }
    [[nodiscard]] auto instruction_data() const noexcept -> InstructionData const& {
        return instruction_data_;
    }
    [[nodiscard]] auto first_instruction(TaskId const task) const -> InstructionHandle {
        assert(task.index() < task_entries_.size());
        return task_entries_[task.index()];
    }

    /* **************************************** */
    // Planning and allocation
    /* **************************************** */
    // Call once per simulation tick
    void allocate(TaskAllocationContext ctx);

  private:
    void release_assignment(Team team, EntityId entity, TaskId task);
    void add_kill_tasks(ExprHandle condition, Team team, TaskAllocationContext ctx);
    void allocate_free_entities(TaskAllocationContext ctx);
    auto is_trigger_checked(std::uint32_t index) const -> bool;
    void add_checked_trigger_run(TriggerRun run);

    // Immutable
    std::pmr::vector<Instruction> instructions_;
    InstructionData instruction_data_;

    std::pmr::vector<InstructionHandle> task_entries_;

    // Mutable
    AllocationCountdown allocation_countdown_{initial_allocation_countdown};
    TeamArray<std::pmr::vector<TaskId>> active_tasks_;
    TeamEntityArray<Counts> task_entity_allocations_;
    TeamEntityArray<std::pmr::vector<EntityId>> free_entities_;

    // Sorted half-open runs of trigger definition indices, with overlap and adjacency merged.
    std::pmr::vector<TriggerRun> checked_trigger_runs_;
};
}
