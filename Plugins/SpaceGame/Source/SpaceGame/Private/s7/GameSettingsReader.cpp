#include <SpaceGame/s7/GameSettingsReader.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>
#include <ioj/s7/simulation_settings_reader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Misc/Paths.h>

auto read_game_settings(FStringView const path,
                        ioj::levels::LevelDefinition const* const level,
                        USpaceGameLevelConfig const& resources,
                        UObject* const outer) -> std::expected<FLoadedGameSettings, FString> {
    auto const absolute_path{
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FString{path})};
    auto const text{ioj::read_file(std::filesystem::path{*absolute_path})};
    if (!text) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot read settings file '%s': %s"),
                            *absolute_path,
                            UTF8_TO_TCHAR(text.error().system_error.message().c_str()))};
    }

    auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
    auto const assignments{
        ioj::levels::authoring::DefinitionReader{std::filesystem::path{*root}}.read_settings_source(
            *text)};
    auto const report_errors{[&](ioj::levels::Diagnostics const& errors) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot load settings '%s':\n%s"),
                            *absolute_path,
                            UTF8_TO_TCHAR(ioj::levels::format_diagnostics(errors).c_str()))};
    }};
    if (!assignments) {
        return report_errors(assignments.error());
    }

    auto simulation{ioj::s7::resolve_simulation_settings(*assignments, level)};
    if (!simulation) {
        return report_errors(simulation.error());
    }

    auto* const config{DuplicateObject<USpaceGameLevelConfig>(&resources, outer)};
    check(config);
    config->apply_settings(*simulation);

    if (assignments->presentation) {
        config->apply_presentation(*assignments->presentation);
    }
    if (level && level->settings().presentation) {
        config->apply_presentation(*level->settings().presentation);
    }

    return FLoadedGameSettings{.config = config,
                               .simulation = std::move(*simulation),
                               .source = UTF8_TO_TCHAR(text->c_str())};
}
