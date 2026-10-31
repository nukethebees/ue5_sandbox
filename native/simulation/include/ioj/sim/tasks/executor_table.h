#pragma once

#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/entity_table_operations.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_id.h>
#include <ioj/subspan.h>

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <utility>
#include <vector>

namespace ioj::sim::tasks {
struct ExecutorConstView {
    EntityType entity_type{};
    std::span<TaskId const> current_tasks;
    std::span<TaskId const> pending_tasks;
    std::span<InstructionHandle const> current_instructions;
};

struct ExecutorView {
    EntityType entity_type{};
    std::span<TaskId> current_tasks;
    std::span<TaskId> pending_tasks;
    std::span<InstructionHandle> current_instructions;
};

struct ExecutorTable : EntityTableOperations {
    using view_type = ExecutorView;
    using const_view_type = ExecutorConstView;

    explicit ExecutorTable(std::pmr::memory_resource& resource = *std::pmr::new_delete_resource());

    void initialise_entities(EntityType type, std::span<EntityId const> ids);

    template <EntityType Type>
    void initialise_entities(std::span<EntityId const> const ids) {
        initialise_entities(Type, ids);
    }

  private:
    friend struct ioj::sim::EntityTableOperations;

    template <typename Self, typename Func>
    decltype(auto) with_columns(this Self& self, EntityType const type, Func&& func) {
        auto const subspan{subspan_for(type)};
        return std::forward<Func>(func)(subspan(self.current_tasks),
                                        subspan(self.pending_tasks),
                                        subspan(self.current_instructions));
    }

    [[nodiscard]] static constexpr auto is_supported_type(EntityType const type) noexcept -> bool {
        return static_cast<std::size_t>(type) < EntityTypeSizes::size();
    }

    [[nodiscard]] static auto subspan_for(EntityType type) -> Subspan;

  public:
    // Keep lifetime slots allocated; index each type's view with EntityId::index().
    std::pmr::vector<TaskId> current_tasks;
    std::pmr::vector<TaskId> pending_tasks;
    std::pmr::vector<InstructionHandle> current_instructions;
};
}
