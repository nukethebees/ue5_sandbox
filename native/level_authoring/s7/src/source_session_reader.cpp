#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_parser.h>
#include <ioj/levels/authoring/source_session.h>

namespace ioj::levels::authoring {
auto resolve_document_settings(std::string_view const base_source,
                               std::string_view const level_source,
                               settings::Settings initial,
                               std::filesystem::path const& base_library_root,
                               std::filesystem::path const& level_library_root)
    -> SettingsReadResult {
    auto base{base_source.empty() ? SettingsReadResult{std::move(initial)}
                                  : DefinitionReader{base_library_root}.read_settings_source(
                                        base_source, std::move(initial))};
    if (!base || level_source.empty()) {
        return base;
    }
    return DefinitionReader{level_library_root}
        .read_level_source(level_source, std::move(*base))
        .transform([](LevelDefinition const& definition) { return definition.settings(); });
}

auto SourceSession::read(settings::Settings base, std::filesystem::path const& library_root) const
    -> std::expected<LevelDefinition, Diagnostics> {
    return read_replay(std::move(base), library_root).transform([](SourceReplay&& replay) {
        return std::move(replay.definition);
    });
}

auto SourceSession::read_replay(settings::Settings base,
                                std::filesystem::path const& library_root) const
    -> std::expected<SourceReplay, Diagnostics> {
    if (!is_attached()) {
        return std::unexpected{Diagnostics{{DiagnosticCode::ExpectedRecord,
                                            "$",
                                            "The source buffer is not attached to a level."}}};
    }

    auto const root{library_root.empty() ? path().parent_path() / "Libraries" : library_root};
    auto ast{DefinitionReader{root}.read_source_ast(buffer())};
    if (!ast) {
        return std::unexpected{std::move(ast.error())};
    }
    auto definition{parse_level(*ast, ast->root, std::move(base))};
    if (!definition) {
        return std::unexpected{std::move(definition.error())};
    }
    return SourceReplay{std::move(*definition), emit_evaluated_source(*ast, ast->root)};
}
}
