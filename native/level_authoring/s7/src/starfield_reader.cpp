#include <ioj/levels/authoring/starfield_reader.h>

#include "reader_diagnostics.h"
#include <ioj/gfx/starfield_settings_reader.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/s7/settings_reader.h>

namespace ioj::levels::authoring {
auto read_starfield_settings_file(std::filesystem::path const& path,
                                  std::filesystem::path const& script_library_root)
    -> std::expected<starfield::Settings, Diagnostics> {
    auto ast{DefinitionReader{script_library_root}.read_script_file(path)};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    auto result{s7::read_settings<starfield::Settings>(s7::Reader{*ast}, ast->root, "starfield")
                    .transform_error(detail::diagnostics)};
    if (!result) {
        for (auto& error : result.error()) {
            error.source_path = path;
        }
    }
    return result;
}
}
