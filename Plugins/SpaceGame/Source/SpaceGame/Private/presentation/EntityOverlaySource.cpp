#include "SpaceGame/presentation/EntityOverlaySource.h"

#include "SpaceGame/entities/TestEntityType.h"

#include <utility>

namespace {
auto inverse_maximum_health(ETestEntityType const type,
                            FEntityOverlayHealthMaximums const& maximums) -> float {
    int32 maximum{0};
    switch (type) {
        case ETestEntityType::CapitalShip: {
            maximum = maximums.capital_ship;
            break;
        }
        case ETestEntityType::CapitalShipFighter: {
            maximum = maximums.fighter;
            break;
        }
        case ETestEntityType::Turret: {
            maximum = maximums.turret;
            break;
        }
        default: {
            return 0.0f;
        }
    }
    return maximum > 0 ? 1.0f / static_cast<float>(maximum) : 0.0f;
}
}

auto collect_entity_overlay_instances(ml::entity_registry::EntityData::ConstView const entities,
                                      FEntityOverlayHealthMaximums const& maximum_health,
                                      FVector3f const origin,
                                      float const maximum_range,
                                      TArray<FEntityOverlayInstance>& output_instances,
                                      FEntityOverlayCollector& collector)
    -> FEntityOverlayCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::CollectRegistrySource);
    entities.validate_array_sizes();
    collector.begin(origin, FMath::Max(maximum_range, 0.0f), output_instances);

    auto const count{entities.num()};
    output_instances.Reserve(count);
    for (int32 index{0}; index < count; ++index) {
        if (entities.alive[index] == 0) {
            continue;
        }

        auto const inverse_health{
            inverse_maximum_health(entities.entity_types[index], maximum_health)};
        if (inverse_health <= 0.0f) {
            continue;
        }

        static_cast<void>(
            collector.try_add(entities.locations[index],
                              static_cast<float>(entities.healths[index]) * inverse_health,
                              entities.radii[index]));
    }

    return {.candidate_count = output_instances.Num(),
            .invalid_health_count = collector.invalid_health_count()};
}
