#pragma once
#include <cstdint>
#include <span>
#include <vector>
struct FTestEntityRegistry;
struct FRegistryEntityHandle;
struct EntityDeathInfo;
namespace ml::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove);

void resolve_damage_events(FTestEntityRegistry const& registry,
                           std::span<FRegistryEntityHandle const> entity_handles,
                           std::span<std::int32_t> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info);

}
