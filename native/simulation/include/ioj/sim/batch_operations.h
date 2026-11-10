#pragma once
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_lookup_table.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/vectors3f.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim {
struct EntityDeathInfo;
}
namespace ioj::sim {
void sort_removal_indices(std::pmr::vector<std::uint32_t>& local_indices_to_remove);

// Consume recipients validated by DamageQueue::prepare while the published rows remain stable.
// Earlier events in this batch may already have killed a recipient.
void resolve_damage_events(DirectDamageEvents::ConstView damage_events,
                           EntityLookupTable const& lookup,
                           [[maybe_unused]] std::span<EntityId const> entity_ids,
                           vectors3f::ResolvedConstView locations,
                           HealthView healths,
                           std::pmr::vector<std::uint32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info);

}
