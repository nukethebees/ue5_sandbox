#include <SpaceGame/s7/GameSettingsReader.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Misc/Paths.h>

auto read_game_settings_source(FStringView const path, USpaceGameLevelConfig const& resources)
    -> std::expected<ioj::settings::Settings, FString> {
    auto const absolute_path{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FString{path})};
    auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
    ioj::levels::authoring::DefinitionReader const reader{std::filesystem::path{*root}};
    ioj::settings::Settings initial;
    initial.presentation = resources.presentation;
    auto settings{
        reader.read_settings_file(std::filesystem::path{*absolute_path}, std::move(initial))};
    if (!settings) {
        return std::unexpected{FString::Printf(
            TEXT("Cannot load settings '%s':\n%s"),
            *absolute_path,
            UTF8_TO_TCHAR(ioj::levels::format_diagnostics(settings.error()).c_str()))};
    }
    return std::move(*settings);
}

auto prepare_level_resources(ioj::settings::Settings const& settings,
                             USpaceGameLevelConfig const& resources,
                             UObject* const outer)
    -> std::expected<USpaceGameLevelConfig*, FString> {
    auto simulation{ioj::sim::resolve_simulation_settings(settings)};
    if (!simulation) {
        return std::unexpected{
            ioj::to_fstring(ioj::levels::format_diagnostics(simulation.error()))};
    }

    auto* const config{DuplicateObject<USpaceGameLevelConfig>(&resources, outer)};
    check(config);
    config->apply_settings(*simulation);

    config->presentation = settings.presentation;
    return config;
}
