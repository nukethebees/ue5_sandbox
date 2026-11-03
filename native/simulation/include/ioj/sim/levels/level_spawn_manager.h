#pragma once
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/transform3d.h>

#include <cstdint>
#include <optional>
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

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct SpawnContext;

class LevelSpawnManager {
    struct FighterSpawnSource {
        EntityId capital_id{};
        float remaining_delay{};
        float cooldown{};
    };

  public:
    LevelSpawnManager(capital_ships::Sim& capital_ships,
                      fighters::Sim& fighters,
                      turrets::Sim& turrets,
                      spinners::Sim& spinners,
                      std::span<Transform3d const> fighter_spawn_transforms);
    LevelSpawnManager(LevelSpawnManager const&) = delete;
    LevelSpawnManager(LevelSpawnManager&&) = delete;
    auto operator=(LevelSpawnManager const&) -> LevelSpawnManager& = delete;
    auto operator=(LevelSpawnManager&&) -> LevelSpawnManager& = delete;

    void initialise(std::uint32_t entity_count,
                    LevelCapitalSpawnEvents::ConstView capital_payloads,
                    LevelTurretSpawnEvents::ConstView turret_payloads,
                    LevelSpinnerSpawnEvents::ConstView spinner_payloads);
    void set_entity_id(LevelEntityIndex entity_index, EntityId id);
    void spawn(LevelSpawnGroups::ConstView groups, SpawnContext context);
    void queue_fighter_spawns(float dt,
                              FrameMemoryResource& scratch_resource,
                              std::span<Team const> participating_teams);
    void reset_tick_output() { spawned_ids_this_tick_.clear(); }
    auto spawned_ids() const -> std::span<EntityId const> { return spawned_ids_this_tick_; }
    auto id(LevelEntityIndex entity_index) const -> EntityId;
    auto entity_ids() const noexcept -> std::span<EntityId const> { return entity_ids_; }
  private:
    void spawn_capitals(LevelCapitalSpawnEvents::ConstView events, SpawnContext context);
    void spawn_turrets(LevelTurretSpawnEvents::ConstView events, SpawnContext context);
    void spawn_spinners(LevelSpinnerSpawnEvents::ConstView events, SpawnContext context);

    LevelCapitalSpawnEvents::ConstView capital_payloads_{};
    LevelTurretSpawnEvents::ConstView turret_payloads_{};
    LevelSpinnerSpawnEvents::ConstView spinner_payloads_{};
    capital_ships::Sim& capital_ships_;
    fighters::Sim& fighters_;
    turrets::Sim& turrets_;
    spinners::Sim& spinners_;
    std::vector<EntityId> entity_ids_{};
    std::vector<EntityId> spawned_ids_this_tick_{};
    std::vector<Transform3d> fighter_spawn_transforms_{};
    std::vector<FighterSpawnSource> fighter_spawn_sources_{};
};
}
