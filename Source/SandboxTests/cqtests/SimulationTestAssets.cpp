#include "SimulationTestAssets.h"

#include <Sandbox/batch_game/SimulationConfig.h>

#include <UObject/SoftObjectPath.h>
#include <UObject/SoftObjectPtr.h>


namespace ml {
auto load_default_simulation_config() -> USimulationConfig const* {
    static TSoftObjectPtr<USimulationConfig> const default_config{
        FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                             "DA_default_simulation_config."
                             "DA_default_simulation_config")}};

    return default_config.LoadSynchronous();
}
}
