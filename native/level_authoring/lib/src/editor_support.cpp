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
    if (definition.metadata().par_time_seconds) {
        errors.emplace_back(DiagnosticCode::UnsupportedEditorFeature,
                            "level.par-time",
                            "Editor level source does not support par-time");
    }
    if (!definition.unlock_level_ids().empty()) {
        errors.emplace_back(DiagnosticCode::UnsupportedEditorFeature,
                            "level.unlock",
                            "Editor level source does not support unlock criteria");
    }
    if (!definition.mission_events().empty()) {
        errors.emplace_back(DiagnosticCode::UnsupportedEditorFeature,
                            "level.mission-events",
                            "Editor level source does not support mission events");
    }
    check_symbol(errors, definition.metadata().id.value, "level.id");
    // Semantic validation already requires every reference to name one of these entities.
    auto const count{definition.entities().size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& entity{definition.entities()[index]};
        auto const path{std::format("level.entities[{}]", index)};
        check_symbol(errors, entity.id.value, path + ".id");
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}
}
