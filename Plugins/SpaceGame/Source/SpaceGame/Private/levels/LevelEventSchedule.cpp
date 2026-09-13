#include <SpaceGame/levels/CompileLevelEvents.h>

#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>

#include <Containers/StringConv.h>
#include <sandbox/simulation/levels/LevelCompilation.h>

namespace ml {
namespace {
auto to_compilation_fstring(std::string const& value) -> FString {
    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}
} // namespace

auto compile_level_events(FLevelDefinition const& definition,
                          FSimulationClock const& clock,
                          FCapitalSimulationConfig const& capital_config,
                          FTurretSimulationConfig const& turret_config)
    -> FLevelEventCompilationResult {
    auto result{level_authoring::compile_level(
        level_authoring::to_native(definition), clock, capital_config, turret_config)};
    if (result) {
        return FLevelEventCompilationResult{std::in_place, std::move(result.value())};
    }

    FLevelStartErrors errors;
    for (auto const& error : result.error()) {
        errors.add(to_compilation_fstring(error));
    }
    return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
}
} // namespace ml
