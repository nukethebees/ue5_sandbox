#pragma once

#include <ioj/sim/entities/entity_lookup_tables.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/tasks/executor_table.h>

namespace ioj::sim {
struct EntityTables {
    explicit EntityTables(SimClock const& clock,
                          std::pmr::memory_resource& memory = *std::pmr::get_default_resource())
        : lookups{clock, memory}
        , health{memory}
        , executors{memory} {}

    template <EntityType Type>
    void publish(std::span<EntityId const> const ids,
                 std::span<Team const> const teams,
                 Health const maximum) {
        auto& table{lookups.for_type(Type)};
        assert(is_preparation_mutation_allowed(lookups.clock()));
        if constexpr (is_damageable(Type)) {
            auto const healths{health.const_view<Type>()};
            table.publish_rows(ids, teams, healths, maximum);
        } else {
            table.publish_rows(ids, teams);
        }
    }

    EntityLookupTables lookups;
    HealthTable health;
    tasks::ExecutorTable executors;
};
}
