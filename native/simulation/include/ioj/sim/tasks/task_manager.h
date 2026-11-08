#pragma once

#include <ioj/sim/entities/team_entity_array.h>
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

namespace ioj::sim::tasks {
struct TaskAllocationContext;

class TaskManager {
  public:
    using Counts = std::pmr::vector<TaskEntityCount>;

    static constexpr TaskId idle_task{0};
    static constexpr InstructionHandle idle_instruction{0};
    static constexpr Instruction jump_to_idle{Instruction::make_data_jump(idle_instruction)};

    /* **************************************** */
    // Lifecycle
    /* **************************************** */
    // Keep the resource alive until after this manager and its storage are destroyed.
    explicit TaskManager(std::pmr::memory_resource& resource = *std::pmr::new_delete_resource());

    // Invalidates custom task and instruction handles while retaining allocated storage.
    void reset();

    /* **************************************** */
    // Task registration
    /* **************************************** */
    void reserve_instructions(std::size_t count);
    // Task instructions must end with jump_to_idle. Registration is independent of team activation.
    [[nodiscard]] auto add_task(std::span<Instruction const> instructions) -> TaskId;
    [[nodiscard]] auto add_task(std::initializer_list<Instruction> instructions) -> TaskId;

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
    void allocate(TaskAllocationContext ctx);

  private:
    // Immutable
    std::pmr::vector<Instruction> instructions_;
    InstructionData instruction_data_;

    std::pmr::vector<InstructionHandle> task_entries_;

    // Mutable
    TeamArray<std::pmr::vector<TaskId>> active_tasks_;
    TeamEntityArray<Counts> task_entity_allocations_;
};
}
