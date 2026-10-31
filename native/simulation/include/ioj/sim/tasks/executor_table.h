#pragma once

#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_id.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim::tasks {
struct ExecutorConstView {
    std::span<TaskId const> current_tasks;
    std::span<TaskId const> pending_tasks;
    std::span<InstructionHandle const> current_instructions;
};

struct ExecutorView {
    std::span<TaskId> current_tasks;
    std::span<TaskId> pending_tasks;
    std::span<InstructionHandle> current_instructions;
};

struct ExecutorTable {
    ExecutorTable();

    // Views use EntityInstanceHandle::index() within the selected entity type.
    [[nodiscard]] auto get_const_view(EntityType type, std::size_t count) const
        -> ExecutorConstView;
    [[nodiscard]] auto get_view(EntityType type, std::size_t count) -> ExecutorView;

    template <EntityType Type>
    [[nodiscard]] auto get_const_view(std::size_t const count) const -> ExecutorConstView {
        return get_const_view(Type, count);
    }

    template <EntityType Type>
    [[nodiscard]] auto get_view(std::size_t const count) -> ExecutorView {
        return get_view(Type, count);
    }

    void initialise_rows(EntityType type,
                         EntityFrameIndex::value_type first,
                         EntityFrameIndex::value_type count);
    // Remove descending owner rows by swapping each with the remaining last row.
    void remove_rows(EntityType type,
                     std::size_t count,
                     std::span<EntityFrameIndex::value_type const> rows);
    void apply_permutation(EntityType type, std::span<std::int32_t> order);

    template <EntityType Type>
    void initialise_rows(EntityFrameIndex::value_type const first,
                         EntityFrameIndex::value_type const count) {
        initialise_rows(Type, first, count);
    }

    template <EntityType Type>
    void remove_rows(std::size_t const count,
                     std::span<EntityFrameIndex::value_type const> const rows) {
        remove_rows(Type, count, rows);
    }

    template <EntityType Type>
    void apply_permutation(std::span<std::int32_t> const order) {
        apply_permutation(Type, order);
    }

    // Keep columns at their preallocated size; use views for active rows.
    std::vector<TaskId> current_tasks;
    std::vector<TaskId> pending_tasks;
    std::vector<InstructionHandle> current_instructions;
};
}
