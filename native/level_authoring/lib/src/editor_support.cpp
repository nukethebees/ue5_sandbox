#include <ioj/levels/authoring/editor_support.h>

#include <ioj/levels/authoring/identifiers.h>

#include <format>

namespace ioj::levels::authoring {
namespace {
void check_symbol(Diagnostics& errors, std::string_view const value, std::string path) {
    if (!is_canonical_symbol(value)) {
        errors.emplace_back(DiagnosticCode::InvalidSymbol,
                            std::move(path),
                            std::format("'{}' is not a canonical lowercase Lisp symbol", value));
    }
}

}

auto check_editor_support(LevelDefinition const& level) -> std::expected<void, Diagnostics> {
    auto const& definition{level};
    Diagnostics errors;
    auto const unsupported = [&](std::string path, std::string_view const feature) {
        errors.emplace_back(DiagnosticCode::UnsupportedEditorFeature,
                            std::move(path),
                            std::format("Editor level source does not support {}", feature));
    };

    if (definition.metadata().par_time_seconds) {
        unsupported("level.par-time", "par-time");
    }
    if (!definition.unlock_level_ids().empty()) {
        unsupported("level.unlock", "unlock criteria");
    }
    if (definition.mission()) {
        unsupported("level.mission", "objective editing; edit the S7 mission source directly");
    }
    auto const& settings{definition.settings()};
    if (settings) {
        unsupported("level.settings", "settings layers");
    }
    check_symbol(errors, definition.metadata().id.value, "level.id");
    // Semantic validation already requires every reference to name one of these entities.
    auto const count{definition.entities().size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& entity{definition.entities()[index]};
        auto const path{std::format("level.entities[{}]", index)};
        check_symbol(errors, entity.id.value, path + ".id");
        if (!entity.overrides.empty()) {
            unsupported(path, "entity overrides");
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}
}
