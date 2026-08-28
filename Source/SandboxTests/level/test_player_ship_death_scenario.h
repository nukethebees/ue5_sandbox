#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/TestEntityUniqueId.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

class ATestSpaceShip;

namespace ml {
class FTestPlayerShipDeathScenario final : public FSimulationTestScenario {
    struct FSimulationSample {
        bool player_handle_is_dead{false};
        bool player_actor_is_valid{false};
        bool player_unique_entity_is_alive{false};
    };

    inline static FTimespan const timeout{0, 0, 2};
    static constexpr double kill_time{0.1};
    static constexpr double post_kill_time{0.4};
  public:
    explicit FTestPlayerShipDeathScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void player_ship_pre_begin_play(UWorld& world, USpaceGameLevelConfig const& config);
    void player_ship_post_orchestrator_spawn(UWorld& world,
                                             USpaceGameLevelConfig const& config,
                                             ATestBatchOrchestrator& orchestrator);
    void queue_player_ship_death();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void check_player_ship_death();

    TWeakObjectPtr<ATestSpaceShip> player_ship{nullptr};
    FRegistryEntityHandle player_ship_handle{};
    TestEntityUniqueId player_ship_id{};
    TimeSeriesData<FSimulationSample> samples;
};
}
