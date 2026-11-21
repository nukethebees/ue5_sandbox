#include <ioj/levels/authoring/game_configuration.h>

#include "reader_diagnostics.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/settings/application_settings_reader.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_settings_state_reader.h>
#include <ioj/settings/game_settings_state_writer.h>

#include <fstream>
#include <random>
#include <string_view>

namespace ioj::levels::authoring {
namespace {
template <class T, class Decode>
auto decode_config(DefinitionReader const& reader,
                   std::filesystem::path const& path,
                   char const* export_name,
                   Decode decode) -> std::expected<T, std::string> {
    auto ast{reader.read_script_file(path, export_name)};
    if (!ast) {
        return std::unexpected{format_diagnostics(ast.error())};
    }
    auto result{decode(s7::Reader{*ast}, ast->root, export_name)};
    if (!result) {
        return std::unexpected{
            std::format("{}: {}",
                        path_to_utf8(path),
                        format_diagnostics(detail::diagnostics(std::move(result.error()))))};
    }
    return *result;
}

template <class T>
auto read_config(DefinitionReader const& reader,
                 std::filesystem::path const& path,
                 char const* export_name) -> std::expected<T, std::string> {
    return decode_config<T>(
        reader, path, export_name, [](auto const& fields, auto root, auto name) {
            return s7::read<T>(fields, root, name);
        });
}

auto read_preferences_override(DefinitionReader const& reader,
                               std::filesystem::path const& path,
                               GameSettingsState const& defaults) {
    return decode_config<GameSettingsState>(
        reader, path, "preferences", [&](auto const& fields, auto root, auto name) {
            return s7::read_override(fields, root, name, defaults);
        });
}

auto write_preferences(std::filesystem::path const& path, std::string_view source)
    -> SavePreferencesResult {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return SavePreferencesResult{std::unexpect,
                                     std::format("{}: {}", path_to_utf8(path), error.message())};
    }
    auto temporary{path};
    temporary += std::format(".{:x}.tmp", std::random_device{}());
    std::ofstream output{temporary, std::ios::binary | std::ios::noreplace};
    if (!output) {
        return SavePreferencesResult{std::unexpect,
                                     std::format("Cannot create {}", path_to_utf8(temporary))};
    }
    output << source;
    output.close();
    if (output) {
        std::filesystem::rename(temporary, path, error);
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    if (!output || error) {
        return SavePreferencesResult{std::unexpect,
                                     std::format("Cannot save {}: {}",
                                                 path_to_utf8(path),
                                                 error ? error.message() : "write failed")};
    }
    return {};
}
}

auto read_application_settings(std::filesystem::path const& script_root)
    -> ApplicationSettingsResult {
    return read_config<ApplicationSettings>(
        DefinitionReader{script_root}, script_root / "Settings/application.scm", "app-settings");
}

auto read_user_settings(std::filesystem::path const& script_root,
                        std::filesystem::path const& personal_file,
                        ScreenResolution desktop) -> UserSettingsResult {
    DefinitionReader const reader{script_root};
    auto defaults{read_config<GameSettingsState>(
        reader, script_root / "Settings/user-defaults.scm", "preferences")};
    if (!defaults) {
        return UserSettingsResult{std::unexpect, std::move(defaults.error())};
    }
    if (defaults->resolution == ScreenResolution{}) {
        defaults->resolution = desktop;
    }
    defaults->overall_quality = graphics_preset(*defaults);

    std::error_code error;
    if (!std::filesystem::exists(personal_file, error)) {
        if (error) {
            return UserSettingsResult{
                std::unexpect, std::format("{}: {}", path_to_utf8(personal_file), error.message())};
        }
        auto created{write_preferences(
            personal_file, "(export preferences)\n(define preferences (user-settings))\n")};
        if (!created) {
            return UserSettingsResult{std::unexpect, std::move(created.error())};
        }
    }
    auto applied{read_preferences_override(reader, personal_file, *defaults)};
    if (!applied) {
        return UserSettingsResult{std::unexpect, std::move(applied.error())};
    }
    applied->overall_quality = graphics_preset(*applied);
    return UserSettingsResult{std::in_place, *defaults, *applied};
}

auto save_user_settings(std::filesystem::path const& personal_file,
                        GameSettingsState const& state,
                        GameSettingsState const& defaults) -> SavePreferencesResult {
    lispb::Strings strings;
    s7::SExprIR ir{strings};
    auto const form{s7::write(ir, state, &defaults)};
    lispb::codegen::sexpr::SExprRenderer renderer{ir, lispb::codegen::sexpr::Dialect::scheme};
    return write_preferences(personal_file,
                             "(export preferences)\n(define preferences " + renderer.render(form) +
                                 ")\n");
}
}
