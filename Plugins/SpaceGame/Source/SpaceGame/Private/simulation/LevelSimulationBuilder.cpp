#include "SpaceGame/simulation/LevelSimulationBuilder.h"

#include <ioj/sim/levels/level_simulation_builder.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj {
auto prepare_level_simulation(FLevelSimulationRequest const& request) -> FLevelSimBuildResult {
    auto const& config{request.config};
    auto const has_player{IsValid(request.player)};
    auto const presentation_enabled{request.launch_options.presentation_mode ==
                                    levels::PresentationMode::Visual};
    TArray<FString> messages;
    config.get_validation_errors(
        messages, presentation_enabled, request.definition.settings().presentation);
    FLevelStartErrors errors;
    errors.append(MoveTemp(messages));
    if (presentation_enabled && has_player && !config.player_ship.team_visual_data) {
        errors.add(TEXT("player_ship.team_visual_data is null"));
    }
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    sim::LevelSimInitData inputs;
    if (has_player) {
        inputs.player = request.player->make_spawn_data();
    }
    auto built{sim::make_level_simulation_init_data(
        request.definition, std::move(inputs), request.timing)};
    if (!built) {
        for (auto const& message : built.error()) {
            errors.add(to_fstring(message));
        }
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return FLevelSimBuildResult{std::in_place, std::move(*built)};
}

}
