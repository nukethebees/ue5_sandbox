#include <SpaceGame/levels/CompileLevelEvents.h>

#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

#include <ioj/sim/levels/level_compilation.h>
#include <SandboxCoreEngine/strings.h>

namespace ml {
auto compile_level_events(FLevelDefinition const& definition,
                          ::ioj::sim::SimClock const& clock,
                          ::ioj::sim::CapitalShipSimConfig const& capital_config,
                          ::ioj::sim::TurretSimConfig const& turret_config)
    -> FLevelEventCompilationResult {
    auto result{::ioj::sim::levels::compile_level(
        level_authoring::to_native(definition), clock, capital_config, turret_config)};
    if (result) {
        return FLevelEventCompilationResult{std::in_place, std::move(result.value())};
    }

    FLevelStartErrors errors;
    for (auto const& error : result.error()) {
        errors.add(to_fstring(error));
    }
    return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
}
} // namespace ml
