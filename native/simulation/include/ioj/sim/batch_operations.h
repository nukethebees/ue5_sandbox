#pragma once
#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/health.h>

#include <cstdint>
#include <span>
#include <vector>
struct EntityRegistry;
struct RegistryEntityHandle;
struct EntityDeathInfo;
namespace ioj::sim::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove);

void resolve_damage_events(EntityRegistry const& registry,
                           AgentIndexes const& indexes,
                           std::span<RegistryEntityHandle const> entity_handles,
                           std::span<EntityUniqueId const> entity_ids,
                           std::span<Health> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info);

}
