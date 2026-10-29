#include <SpaceGame/s7/GameSettingsReader.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/level_definition.h>
#include <ioj/s7/simulation_settings_reader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Misc/Paths.h>

auto read_game_settings_source(FStringView const path, USpaceGameLevelConfig const& resources)
    -> std::expected<FLoadedSettingsSource, FString> {
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
    initial.presentation = resources.presentation;
    auto const ast{reader.read_source_ast(*text)};
    auto settings{ast.and_then([&](ioj::s7::Ast const& value) {
        return ioj::levels::authoring::parse_settings(value, value.root, std::move(initial));
    })};
    if (!settings) {
        return std::unexpected{FString::Printf(
            TEXT("Cannot load settings '%s':\n%s"),
            *absolute_path,
            UTF8_TO_TCHAR(ioj::levels::format_diagnostics(settings.error()).c_str()))};
    }
    auto const evaluated{ioj::levels::authoring::emit_evaluated_source(*ast, ast->root)};
    return FLoadedSettingsSource{.settings = std::move(*settings),
                                 .source = UTF8_TO_TCHAR(text->c_str()),
                                 .evaluated_source = ioj::to_fstring(evaluated)};
}

auto read_game_settings(FStringView const path,
                        ioj::levels::authoring::LevelSource const* const level,
                        USpaceGameLevelConfig const& resources,
                        UObject* const outer) -> std::expected<FLoadedGameSettings, FString> {
    auto const report_errors{[&](ioj::levels::Diagnostics const& errors) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot resolve settings '%s':\n%s"),
                            *FString{path},
                            UTF8_TO_TCHAR(ioj::levels::format_diagnostics(errors).c_str()))};
    }};

    FLoadedSettingsSource settings;
    std::optional<ioj::levels::LevelDefinition> decoded_level;
    if (level && std::holds_alternative<ioj::levels::LevelDefinition>(*level)) {
        decoded_level = std::get<ioj::levels::LevelDefinition>(*level);
    } else {
        auto loaded{read_game_settings_source(path, resources)};
        if (!loaded) {
            return std::unexpected{std::move(loaded.error())};
        }
        settings = std::move(*loaded);

        if (level) {
            auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
            ioj::levels::authoring::DefinitionReader const reader{std::filesystem::path{*root}};
            auto decoded{reader.resolve_level(*level, settings.settings)};
            if (!decoded) {
                return report_errors(decoded.error());
            }
            decoded_level = std::move(*decoded);
        }
    }
    auto const& effective{decoded_level ? decoded_level->settings() : settings.settings};
    auto simulation{ioj::s7::resolve_simulation_settings(effective)};
    if (!simulation) {
        return report_errors(simulation.error());
    }

    auto* const config{DuplicateObject<USpaceGameLevelConfig>(&resources, outer)};
    check(config);
    config->apply_settings(*simulation);

    config->presentation = effective.presentation;
    auto const* const level_text{level ? std::get_if<std::string>(level) : nullptr};
    return FLoadedGameSettings{.config = config,
                               .simulation = std::move(*simulation),
                               .source = std::move(settings.source),
                               .level_source =
                                   level_text ? ioj::to_fstring(*level_text) : FString{},
                               .level = std::move(decoded_level)};
}
