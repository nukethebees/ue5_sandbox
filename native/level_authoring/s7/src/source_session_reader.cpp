#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/source_session.h>

namespace ioj::levels::authoring {
auto SourceSession::read(settings::Settings base, std::filesystem::path const& library_root) const
    -> std::expected<LevelDefinition, Diagnostics> {
    if (!is_attached()) {
        return std::unexpected{Diagnostics{{DiagnosticCode::ExpectedRecord,
                                            "$",
                                            "The source buffer is not attached to a level."}}};
    }
    auto const root{library_root.empty() ? path().parent_path() : library_root};
    return DefinitionReader{root}.read_level_source(buffer(), std::move(base));
}
}
