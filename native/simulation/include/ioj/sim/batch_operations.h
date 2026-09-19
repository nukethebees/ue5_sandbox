#pragma once
#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/health_table.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct EntityDeathInfo;
class EntityLedger;
}
namespace ioj::sim::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove);

void resolve_damage_events(DirectDamageEventsConstView damage_events,
                           AgentIndexes const& indexes,
                           [[maybe_unused]] std::span<EntityUniqueId const> entity_ids,
                           HealthView healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info,
                           EntityLedger& ledger);

}
