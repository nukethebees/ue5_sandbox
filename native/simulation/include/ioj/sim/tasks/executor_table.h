#pragma once

#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/entity_table_operations.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_id.h>
#include <ioj/subspan.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
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

struct ExecutorTable : EntityTableOperations {
    using view_type = ExecutorView;
    using const_view_type = ExecutorConstView;

    ExecutorTable();

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

  private:
    friend struct ioj::sim::EntityTableOperations;

    template <typename Self, typename Func>
    decltype(auto)
        with_columns(this Self& self, EntityType const type, std::size_t const count, Func&& func) {
        auto const subspan{subspan_for(type, count)};
        return std::forward<Func>(func)(subspan(self.current_tasks),
                                        subspan(self.pending_tasks),
                                        subspan(self.current_instructions));
    }

    [[nodiscard]] static constexpr auto is_supported_type(EntityType const type) noexcept -> bool {
        return static_cast<std::size_t>(type) < EntityTypeSizes::size();
    }

    [[nodiscard]] static auto subspan_for(EntityType type, std::size_t count) -> Subspan;

  public:
    // Keep columns at their preallocated size; use views for active rows.
    std::vector<TaskId> current_tasks;
    std::vector<TaskId> pending_tasks;
    std::vector<InstructionHandle> current_instructions;
};
}
