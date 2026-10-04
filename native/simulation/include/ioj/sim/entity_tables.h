#pragma once

#include <ioj/sim/entity_lookup_table.h>
#include <ioj/sim/health_table.h>

namespace ioj::sim {
struct EntityTables {
    explicit EntityTables(SimClock const& clock)
        : lookups{clock} {}

    template <EntityType Type>
    void publish(std::span<EntityUniqueId const> const ids,
                 std::span<Team const> const teams,
                 Health const maximum) {
        auto& table{lookups.for_type(Type)};
        lookups.assert_preparation_mutation_allowed();
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
