#pragma once
#include <sandbox/simulation/capital_death_event.h>
#include <sandbox/simulation/laser_hit_details.h>

#include <sandbox/simulation/laser_soa.h>
#include <sandbox/simulation/sim_tick.h>
#include <sandbox/simulation/spinner_entity_data.h>
#include <sandbox/simulation/turret_entity_data.h>
struct FTestEntityRegistry;
#include <sandbox/simulation/capital_entity_data.h>
#include <sandbox/simulation/fighter_entity_data.h>

#include <span>

enum class EEntityFrameChange : std::uint8_t { Spawn, RemoveSwap };

struct FEntityFrameChange {
    EEntityFrameChange kind{};
    std::int32_t index{};
    ml::simulation::Vector3f location{};
    ml::simulation::Rotator3f rotation{};
    ml::simulation::Team team{};
    FRegistryEntityHandle handle{};
};

using FCapitalDeathEvent = ml::simulation::CapitalDeathEvent;

struct FCapitalReadView {
    ml::simulation::CapitalEntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    std::span<FRegistryEntityHandle const> fighter_handles;
    std::span<FEntityFrameChange const> changes;
    std::span<FCapitalDeathEvent const> deaths;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
    auto get_fighter_handles(std::int32_t index) const -> std::span<FRegistryEntityHandle const> {
        auto const span{entities.capital_fighter_handle_spans[index]};
        return fighter_handles.subspan(span.offset, span.count);
    }
};
struct FFighterReadView {
    ml::simulation::FighterEntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct FTurretReadView {
    ml::simulation::TurretEntityData::ConstView entities;
    FTestEntityRegistry const* registry{};
    std::span<FEntityFrameChange const> changes;
    std::span<ml::simulation::Vector3f const> death_locations;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct FSpinnerReadView {
    ml::simulation::SpinnerEntityData::ConstView entities;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
struct FLaserReadView {
    ml::simulation::lasers::Entities::ConstView entities;
    ml::simulation::LaserHitDetailsConstView hits;
    std::span<ml::simulation::SimTick const> hit_ticks;
    std::span<std::int32_t const> hit_ordinals;
    auto get_num_instances() const -> std::int32_t { return entities.num(); }
};
