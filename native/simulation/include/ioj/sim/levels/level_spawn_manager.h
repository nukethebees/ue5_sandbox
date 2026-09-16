#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/levels/level_runtime_events.h>

#include <ioj/sim/entity_handle.h>

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
                    LevelCapitalSpawnEventsConstView capital_payloads,
                    LevelTurretSpawnEventsConstView turret_payloads);
    void set_entity_handle(std::int32_t entity_index, RegistryEntityHandle handle);
    void spawn_initial(LevelCapitalSpawnEventsConstView capital_events,
                       LevelTurretSpawnEventsConstView turret_events,
                       LevelSpinnerSpawnEventsConstView spinner_events);
    void spawn(LevelSpawnGroupsConstView groups);
    void reset_tick_output() { spawned_handles_this_tick_.clear(); }
    auto get_spawned_handles() const -> std::span<RegistryEntityHandle const> {
        return spawned_handles_this_tick_;
    }
    auto get_handle(std::int32_t entity_index) const -> RegistryEntityHandle;
    auto get_entity_handles() const noexcept -> std::span<RegistryEntityHandle const> {
        return entity_handles_;
    }
  private:
    void spawn_capitals(LevelCapitalSpawnEventsConstView events);
    void resolve_capital_targets(LevelCapitalSpawnEventsConstView events);
    void spawn_turrets(LevelTurretSpawnEventsConstView events);
    void spawn_spinners(LevelSpinnerSpawnEventsConstView events);

    LevelCapitalSpawnEventsConstView capital_payloads_{};
    LevelTurretSpawnEventsConstView turret_payloads_{};
    capital_ships::Sim& capital_ships_;
    turrets::Sim& turrets_;
    spinners::Sim& spinners_;
    std::vector<RegistryEntityHandle> entity_handles_{};
    std::vector<EntityUniqueId> target_ids_scratch_{};
    std::vector<RegistryEntityHandle> spawned_handles_this_tick_{};
};
}
