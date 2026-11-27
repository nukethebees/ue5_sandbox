#pragma once
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/team_entity_array.h>
#include <ioj/sim/fighters/fighter_spawn_queue.h>
#include <ioj/sim/fighters/phase_inputs.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/levels/fighter_launch_column.h>
#include <ioj/sim/levels/level_entity_ref.h>
#include <ioj/sim/levels/level_spawn_tables.h>
#include <ioj/team_array.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim::capital_ships {
struct Sim;
}

namespace ioj::sim::turrets {
struct Sim;
}

namespace ioj::sim::spinners {
struct Sim;
}

namespace ioj::sim::fighters {
struct Sim;
}

namespace ioj::sim {
class LevelEventManager;
struct EntityInstanceHandle;
struct SpawnContext;

struct EntityFrameChanges;
struct LevelSpawnContext {
    EntityFrameChanges& capital_changes;
    EntityFrameChanges& turret_changes;
    capital_ships::Sim& capitals;
    turrets::Sim& turrets;
    spinners::Sim& spinners;
    fighters::Sim& fighters;
    fighters::PreparationInputs fighter_inputs;
};

// Spawn authored entities. Schedule carrier fighter waves.
class LevelSpawnManager {
    using SpawnRequestCount = std::uint16_t;

    struct CarrierLaunch {
        Team team;
        FighterLaunchColumn column;
    };

    struct FighterSpawnSource {
        EntityId capital_id{};
        float remaining_delay{};
        float cooldown{};
    };

  public:
    LevelSpawnManager(std::size_t fighters_per_carrier_launch,
                      EntityCount max_live_carrier_fighters,
                      TeamCount team_count,
                      float clearance_buffer,
                      std::pmr::memory_resource& memory);
    LevelSpawnManager(LevelSpawnManager const&) = delete;
    LevelSpawnManager(LevelSpawnManager&&) = delete;
    auto operator=(LevelSpawnManager const&) -> LevelSpawnManager& = delete;
    auto operator=(LevelSpawnManager&&) -> LevelSpawnManager& = delete;

    void initialise(std::uint32_t entity_count);
    void set_entity_id(LevelEntityRef entity_index, EntityId id);
    void process_event(EventHandle event,
                       LevelEventManager const& events,
                       LevelSpawnContext const& level);
    void queue_fighter_spawns(float dt,
                              CapitalEntityData::CompactConstView capitals,
                              std::span<EntityInstanceHandle const> capital_handles,
                              collision::EntityAABBs const& bounds);
    void commit_carrier_launches(LevelSpawnContext const& level);
    auto entity_ids() const noexcept -> std::span<EntityId const> { return entity_ids_; }

  private:
    void spawn_fighters(FighterSpawnQueue::CompactConstView placements,
                        LevelSpawnContext const& level);
    void spawn_carrier_fighters(CarrierLaunch const& launch,
                                SpawnRequestCount count,
                                LevelSpawnContext const& level);
    void spawn_capitals(LevelCapitalSpawnEvents::CompactConstView events,
                        capital_ships::Sim& capital_ships,
                        SpawnContext context,
                        EntityFrameChanges& changes);
    void spawn_turrets(LevelTurretSpawnEvents::CompactConstView events,
                       turrets::Sim& turrets,
                       SpawnContext context,
                       EntityFrameChanges& changes);
    void spawn_spinners(LevelSpinnerSpawnEvents::CompactConstView events,
                        spinners::Sim& spinners,
                        SpawnContext context);

    std::pmr::vector<EntityId> entity_ids_;
    SpawnRequestCount fighters_per_carrier_launch_;
    EntityCount max_live_carrier_fighters_per_team_;
    std::pmr::vector<CarrierLaunch> carrier_launches_;
    FighterSpawnQueue launch_placements_;
    float clearance_buffer_;
    std::pmr::vector<FighterSpawnSource> fighter_spawn_sources_;
};
}
