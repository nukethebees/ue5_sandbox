#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_lookup_table.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/vectors3f.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct EntityDeathInfo;
class EntityLedger;
}
namespace ioj::sim {
void sort_and_deduplicate_removal_indices(std::vector<std::uint32_t>& local_indices_to_remove);

// Consume recipients validated by DamageQueue::prepare while the published rows remain stable.
// Earlier events in this batch may already have killed a recipient.
void resolve_damage_events(DirectDamageEvents::ConstView damage_events,
                           EntityLookupTable const& lookup,
                           [[maybe_unused]] std::span<EntityUniqueId const> entity_ids,
                           Vectors3fConstView locations,
                           HealthView healths,
                           std::vector<std::uint32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info,
                           EntityLedger& ledger,
                           ioj::FrameMemoryResource* scratch_resource);

}
