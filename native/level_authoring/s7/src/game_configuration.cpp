#include <ioj/levels/authoring/game_configuration.h>

#include "reader_diagnostics.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/s7/writer.h>
#include <ioj/settings/application_settings.h>
#include <ioj/settings/application_settings_reader.h> // IWYU pragma: keep
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/game_settings_state_reader.h> // IWYU pragma: keep
#include <ioj/settings/game_settings_state_writer.h>
#include <ioj/settings/screen_resolution.h>

#include <codegen/sexpr/dialect.h>
#include <codegen/sexpr/sexpr_renderer.h>
#include <lispb/strings.h>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ioj::levels::authoring {
namespace {
template <class T>
auto read_config(DefinitionReader const& reader,
                 std::filesystem::path const& path,
                 char const* export_name) -> std::expected<T, std::string> {
    using ConfigResult = std::expected<T, std::string>;

    auto ast{reader.read_script_file(path, export_name)};
    if (!ast) {
        return ConfigResult{std::unexpect, format_diagnostics(ast.error())};
    }

    auto result{s7::read<T>(s7::Reader{*ast}, ast->root, export_name)};
    if (!result) {
        return ConfigResult{
            std::unexpect,
            std::format("{}: {}",
                        path_to_utf8(path),
                        format_diagnostics(detail::diagnostics(std::move(result.error()))))};
    }

    return ConfigResult{std::in_place, std::move(*result)};
}

auto read_preferences(DefinitionReader const& reader,
                      std::filesystem::path const& path,
                      ScreenResolution desktop) {
    auto state{read_config<GameSettingsState>(reader, path, "preferences")};
    if (state) {
        if (state->resolution == ScreenResolution{}) {
            state->resolution = desktop;
        }
        state->overall_quality = graphics_preset(*state);
    }

    return state;
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
    auto defaults{read_preferences(reader, script_root / "Settings/user-defaults.scm", desktop)};
    if (!defaults) {
        return UserSettingsResult{std::unexpect, std::move(defaults.error())};
    }

    std::error_code error;
    if (!std::filesystem::exists(personal_file, error)) {
        if (error) {
            return UserSettingsResult{
                std::unexpect, std::format("{}: {}", path_to_utf8(personal_file), error.message())};
        }

        auto created{save_user_settings(personal_file, *defaults, *defaults)};
        if (!created) {
            return UserSettingsResult{std::unexpect, std::move(created.error())};
        }
    }

    auto applied{read_preferences(reader, personal_file, desktop)};
    if (!applied) {
        return UserSettingsResult{std::unexpect, std::move(applied.error())};
    }

    return UserSettingsResult{std::in_place, *defaults, *applied};
}

auto save_user_settings(std::filesystem::path const& personal_file,
                        GameSettingsState const& state,
                        GameSettingsState const& defaults) -> SavePreferencesResult {
    lispb::Strings strings;
    s7::SExprIR ir{strings};
    auto form{ir.form("copy-with", {ir.atom("defaults")})};
    s7::PropertyWriter writer{ir, form, state, &defaults};
    s7::TypeWriter<GameSettingsState>::properties(writer);
    lispb::codegen::sexpr::SExprRenderer renderer{ir, lispb::codegen::sexpr::Dialect::scheme};
    return write_preferences(personal_file,
                             "(export preferences)\n"
                             "(define defaults (module-ref (import-module "
                             "\"Settings/user-defaults.scm\") 'preferences))\n"
                             "(define preferences " +
                                 renderer.render(form.build()) + ")\n");
}
}
