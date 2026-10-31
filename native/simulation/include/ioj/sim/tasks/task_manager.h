#pragma once

#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_count.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/program_handle.h>
#include <ioj/sim/tasks/programs.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/tasks/tasks.h>

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim::tasks {
class TaskManager {
  public:
    static constexpr ProgramHandle idle_program{0};

    // Keep the resource alive until after this manager and its storage are destroyed.
    explicit TaskManager(std::pmr::memory_resource& resource = *std::pmr::new_delete_resource());

    // Invalidates custom program and task handles while retaining allocated storage.
    void reset();
    void reserve_instructions(std::size_t count);
    // Program sizes must be nonzero and fit InstructionCount.
    [[nodiscard]] auto add_program(std::span<Instruction const> program) -> ProgramHandle;
    [[nodiscard]] auto add_program(std::initializer_list<Instruction> program) -> ProgramHandle;

    void plan_tasks();
    void allocate(TaskAllocationContext ctx);

  private:
    [[nodiscard]] static constexpr auto is_valid_program_size(std::size_t const count) noexcept
        -> bool {
        return count != 0 && count <= (std::numeric_limits<InstructionCount>::max)();
    }

    [[nodiscard]] static constexpr auto fits_instruction_range(std::size_t const first,
                                                               std::size_t const count) noexcept
        -> bool {
        return first < InstructionHandle::invalid_value &&
               count <= InstructionHandle::invalid_value - first;
    }

    [[nodiscard]] static constexpr auto is_valid_program_index(std::size_t const index) noexcept
        -> bool {
        return index < ProgramHandle::invalid_value;
    }

    std::pmr::vector<Instruction> instructions_;
    InstructionData instruction_data_;
    Programs programs_;
    Tasks tasks_;
};
}
