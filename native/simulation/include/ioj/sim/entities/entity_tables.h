#pragma once

#include <ioj/sim/entities/entity_lookup_tables.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/sim/phase_checks.h>

namespace ioj::sim {
struct EntityTables {
    explicit EntityTables(SimClock const& clock)
        : lookups{clock} {}

    template <EntityType Type>
    void publish(std::span<EntityId const> const ids,
                 std::span<Team const> const teams,
                 Health const maximum) {
        auto& table{lookups.for_type(Type)};
        assert(is_preparation_mutation_allowed(lookups.clock()));
        if constexpr (has_health(Type)) {
            auto const healths{health.get_const_view<Type>(ids.size()).values()};
            table.publish_rows(ids, teams, healths, maximum);
        } else {
            table.publish_rows(ids, teams);
        }
    }

    EntityLookupTables lookups;
    HealthTable health;
};
}
