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

enum class EntityFrameChangeKind : std::uint8_t { Spawn, RemoveSwap };

struct EntityFrameChange {
    EntityFrameChangeKind kind{};
    std::int32_t index{};
    ioj::sim::Vector3f location{};
    ioj::sim::Rotator3f rotation{};
    ioj::sim::Team team{};
    RegistryEntityHandle handle{};
};

struct CapitalReadView {
    ioj::sim::CapitalEntityData::ConstView entities;
    EntityRegistry const* registry{};
    std::span<RegistryEntityHandle const> fighter_handles;
    std::span<EntityFrameChange const> changes;
    std::span<CapitalDeathEvent const> deaths;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
    auto get_fighter_handles(std::int32_t index) const -> std::span<RegistryEntityHandle const> {
        auto const span{entities.fighter_handle_spans[index]};
        return fighter_handles.subspan(span.offset, span.count);
    }
};
struct FighterReadView {
    ioj::sim::FighterEntityData::ConstView entities;
    EntityRegistry const* registry{};
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct TurretReadView {
    ioj::sim::TurretEntityData::ConstView entities;
    EntityRegistry const* registry{};
    std::span<EntityFrameChange const> changes;
    std::span<ioj::sim::Vector3f const> death_locations;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct SpinnerReadView {
    ioj::sim::SpinnerEntityData::ConstView entities;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct LaserReadView {
    ioj::sim::lasers::Entities::ConstView entities;
    ioj::sim::LaserHitDetailsConstView hits;
    std::span<ioj::sim::SimTick const> hit_ticks;
    std::span<std::int32_t const> hit_ordinals;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
} // namespace ioj::sim
