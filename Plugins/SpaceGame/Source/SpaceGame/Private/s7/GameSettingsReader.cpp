#include <SpaceGame/s7/GameSettingsReader.h>

#include "s7/generated/presentation_fields.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>
#include <ioj/s7/simulation_settings_reader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <Misc/Paths.h>

namespace ioj::s7::game_settings_reader {
template <typename T>
auto assign(settings::Setting const& setting, T& target) -> bool {
    if constexpr (std::same_as<T, FVector2D> || std::same_as<T, FVector3f>) {
        auto const* values{std::get_if<std::vector<double>>(&setting.value)};
        constexpr auto dimensions{std::same_as<T, FVector2D> ? 2u : 3u};
        if (!values || values->size() != dimensions) {
            return false;
        }

        target.X = static_cast<decltype(target.X)>((*values)[0]);
        target.Y = static_cast<decltype(target.Y)>((*values)[1]);
        if constexpr (dimensions == 3) {
            target.Z = static_cast<decltype(target.Z)>((*values)[2]);
        }
        return true;
    } else {
        return setting.assign(target);
    }
}

auto apply_presentation_settings(ioj::settings::Settings const& settings,
                                 USpaceGameLevelConfig& target)
    -> std::expected<void, ioj::levels::Diagnostics> {
    for (auto const& setting : settings) {
        if (!setting.path.starts_with("presentation.")) {
            continue;
        }

        auto const field{presentation_fields::try_parse_fields_field(setting.path)};
        auto const valid{field && presentation_fields::visit_fields_field(
                                      target, *field, [&](auto& target_field) {
                                          return assign(setting, target_field);
                                      })};
        if (!valid) {
            return std::unexpected{
                ioj::levels::Diagnostics{{field ? ioj::levels::DiagnosticCode::InvalidType
                                                : ioj::levels::DiagnosticCode::UnknownProperty,
                                          setting.path,
                                          field ? "Presentation setting has an incompatible value"
                                                : "Unknown presentation setting"}}};
        }
    }
    return {};
}
}

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

    auto presentation{
        ioj::s7::game_settings_reader::apply_presentation_settings(*assignments, *config)};
    if (presentation && level) {
        presentation =
            ioj::s7::game_settings_reader::apply_presentation_settings(level->settings(), *config);
    }
    if (!presentation) {
        return report_errors(presentation.error());
    }

    return FLoadedGameSettings{.config = config,
                               .simulation = std::move(*simulation),
                               .source = UTF8_TO_TCHAR(text->c_str())};
}
