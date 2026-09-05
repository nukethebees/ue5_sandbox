#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>

#include <SandboxNative/RegistryEntityHandle.h>

namespace ml::test_capital_ships {
struct Simulation;
}

namespace ml::test_static_turrets {
struct Simulation;
}

namespace ml {
class SPACEGAME_API FLevelSpawnManager {
  public:
    void initialise(int32 entity_count,
                    test_capital_ships::Simulation& capital_ships,
                    test_static_turrets::Simulation& turrets,
                    FLevelCapitalSpawnEventsConstView capital_payloads,
                    FLevelTurretSpawnEventsConstView turret_payloads);
    void set_entity_handle(int32 entity_index, FRegistryEntityHandle handle);
    void spawn(FLevelSpawnGroupsConstView groups);
    auto get_handle(int32 entity_index) const -> FRegistryEntityHandle;
    auto get_entity_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entity_handles_;
    }
  private:
    void spawn_capitals(FLevelCapitalSpawnEventsConstView events);
    void spawn_turrets(FLevelTurretSpawnEventsConstView events);

    FLevelCapitalSpawnEventsConstView capital_payloads_{};
    FLevelTurretSpawnEventsConstView turret_payloads_{};
    test_capital_ships::Simulation* capital_ships_{};
    test_static_turrets::Simulation* turrets_{};
    TArray<FRegistryEntityHandle> entity_handles_{};
    TArray<FRegistryEntityHandle> target_handles_scratch_{};
};
}
