#pragma once
#include <ioj/sim/capital_death_event.h>
#include <ioj/sim/laser_hit_details.h>

#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/turret_entity_data.h>

#include <span>

namespace ioj::sim {

struct EntityRegistry;
class AgentAccessor;

enum class EntityFrameChangeKind : std::uint8_t { Spawn, RemoveSwap };

struct EntityFrameChange {
    EntityFrameChangeKind kind{};
    std::int32_t index{};
    Vector3f location{};
    Rotator3f rotation{};
    Team team{};
    RegistryEntityHandle handle{};
};

struct CapitalReadView {
    CapitalEntityData::ConstView entities;
    EntityRegistry const* registry{};
    std::span<RegistryEntityHandle const> fighter_handles;
    std::span<EntityFrameChange const> changes;
    std::span<CapitalDeathEvent const> deaths;
    AgentAccessor const* agents{};
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
    auto get_fighter_handles(std::int32_t index) const -> std::span<RegistryEntityHandle const> {
        auto const span{entities.fighter_handle_spans[index]};
        return fighter_handles.subspan(span.offset, span.count);
    }
};
struct FighterReadView {
    FighterEntityData::ConstView entities;
    EntityRegistry const* registry{};
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct TurretReadView {
    TurretEntityData::ConstView entities;
    EntityRegistry const* registry{};
    std::span<EntityFrameChange const> changes;
    std::span<Vector3f const> death_locations;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct SpinnerReadView {
    SpinnerEntityData::ConstView entities;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct LaserReadView {
    lasers::Entities::ConstView entities;
    LaserHitDetailsConstView hits;
    std::span<SimTick const> hit_ticks;
    std::span<std::int32_t const> hit_ordinals;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
} // namespace ioj::sim
