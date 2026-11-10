#include <SpaceGame/levels/LevelCompletion.h>

#include <ioj/sim/levels/level_sim.h>

#include <SandboxCoreEngine/strings.h>
namespace ioj {
auto make_level_completion(sim::LevelSim const& simulation) -> FLevelCompletion {
    auto const result{simulation.result().value()};
    auto const& metadata{simulation.level_metadata()};
    FLevelCompletion completion{.level_id = FName{*to_fstring(metadata.id.value)},
                                .level_display_name = to_fstring(metadata.title),
                                .end_reason = to_fstring(sim::to_serialized_string(result.reason)),
                                .elapsed_time = Duration::make(sim::simulation_time(
                                    result.terminal.tick, simulation.clock().tick_period()))};
    if (result.winner) {
        completion.winner = to_fstring(to_serialized_string(*result.winner));
        auto const* player{simulation.player_ship_simulation()};
        completion.state = !player || player->team == *result.winner ? ELevelOutcome::Victory
                                                                     : ELevelOutcome::Defeat;
    }
    if (metadata.par_time_seconds) {
        completion.par_time_seconds = *metadata.par_time_seconds;
    }
    return completion;
}
}
