#include <SpaceGame/s7/GameSettingsReader.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>
#include <ioj/s7/simulation_settings_reader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Misc/Paths.h>

auto read_game_settings(FStringView const path,
                        ioj::levels::LevelDefinition* const level,
                        FStringView const level_source,
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
    ioj::levels::authoring::DefinitionReader const reader{std::filesystem::path{*root}};
    ioj::settings::Settings initial;
    initial.presentation = {
        .player_ship = resources.player_ship.presentation,
        .laser_projectiles = resources.laser_projectiles.presentation,
        .capital_ships = resources.capital_ships.presentation,
        .turrets = resources.turrets.presentation,
        .entity_overlay = resources.entity_overlay.values,
        .radar = resources.radar,
    };
    auto settings{reader.read_settings_source(*text, std::move(initial))};
    auto const report_errors{[&](ioj::levels::Diagnostics const& errors) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot load settings '%s':\n%s"),
                            *absolute_path,
                            UTF8_TO_TCHAR(ioj::levels::format_diagnostics(errors).c_str()))};
    }};
    if (!settings) {
        return report_errors(settings.error());
    }

    std::optional<ioj::levels::LevelDefinition> decoded_level;
    if (!level_source.IsEmpty()) {
        check(level);
        auto decoded{
            reader.read_level_source(ioj::to_string(FString{level_source}), std::move(*settings))};
        if (!decoded) {
            return report_errors(decoded.error());
        }
        decoded_level = std::move(*decoded);
    }
    auto const* const effective_level{decoded_level ? &*decoded_level : level};
    auto const& effective{
        effective_level && effective_level->settings() ? *effective_level->settings() : *settings};
    auto const* const grid{effective_level && effective_level->collision_grid()
                               ? &*effective_level->collision_grid()
                               : nullptr};
    auto simulation{ioj::s7::resolve_simulation_settings(effective, grid)};
    if (!simulation) {
        return report_errors(simulation.error());
    }

    auto* const config{DuplicateObject<USpaceGameLevelConfig>(&resources, outer)};
    check(config);
    config->apply_settings(*simulation);

    config->apply_presentation(effective.presentation);
    if (decoded_level) {
        *level = std::move(*decoded_level);
    }

    return FLoadedGameSettings{.config = config,
                               .simulation = std::move(*simulation),
                               .source = UTF8_TO_TCHAR(text->c_str())};
}
