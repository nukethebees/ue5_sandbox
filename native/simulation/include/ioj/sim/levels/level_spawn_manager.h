#pragma once
#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/levels/level_runtime_events.h>

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

namespace ioj::sim {
class LevelSpawnManager {
  public:
    LevelSpawnManager(capital_ships::Sim& capital_ships,
                      turrets::Sim& turrets,
                      spinners::Sim& spinners) noexcept;
    LevelSpawnManager(LevelSpawnManager const&) = delete;
    LevelSpawnManager(LevelSpawnManager&&) = delete;
    auto operator=(LevelSpawnManager const&) -> LevelSpawnManager& = delete;
    auto operator=(LevelSpawnManager&&) -> LevelSpawnManager& = delete;

    void initialise(std::int32_t entity_count,
                    SingleAllocationLevelCapitalSpawnEvents::ConstView capital_payloads,
                    SingleAllocationLevelTurretSpawnEvents::ConstView turret_payloads);
    void set_entity_id(std::int32_t entity_index, EntityUniqueId id);
    void spawn_initial(SingleAllocationLevelCapitalSpawnEvents::ConstView capital_events,
                       SingleAllocationLevelTurretSpawnEvents::ConstView turret_events,
                       SingleAllocationLevelSpinnerSpawnEvents::ConstView spinner_events);
    void spawn(LevelSpawnGroupsConstView groups);
    void reset_tick_output() { spawned_ids_this_tick_.clear(); }
    auto get_spawned_ids() const -> std::span<EntityUniqueId const> {
        return spawned_ids_this_tick_;
    }
    auto get_id(std::int32_t entity_index) const -> EntityUniqueId;
    auto get_entity_ids() const noexcept -> std::span<EntityUniqueId const> { return entity_ids_; }
  private:
    void spawn_capitals(SingleAllocationLevelCapitalSpawnEvents::ConstView events);
    void resolve_capital_targets(SingleAllocationLevelCapitalSpawnEvents::ConstView events);
    void spawn_turrets(SingleAllocationLevelTurretSpawnEvents::ConstView events);
    void spawn_spinners(SingleAllocationLevelSpinnerSpawnEvents::ConstView events);

    SingleAllocationLevelCapitalSpawnEvents::ConstView capital_payloads_{};
    SingleAllocationLevelTurretSpawnEvents::ConstView turret_payloads_{};
    capital_ships::Sim& capital_ships_;
    turrets::Sim& turrets_;
    spinners::Sim& spinners_;
    std::vector<EntityUniqueId> entity_ids_{};
    std::vector<EntityUniqueId> target_ids_scratch_{};
    std::vector<EntityUniqueId> spawned_ids_this_tick_{};
};
}
