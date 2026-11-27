#include <ioj/bridge/level_catalog.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/level_definition.h>
#include <ioj/profiles/progression.h>

namespace ioj::bridge {
auto make_catalog_rows(levels::authoring::DefinitionCatalog const& catalog,
                       std::span<profiles::ScoreRecord const> records,
                       bool unlock_all) -> CatalogRows {
    CatalogRows rows;
    auto append_error = [&](std::string const& error) {
        if (!rows.errors.empty()) {
            rows.errors += '\n';
        }
        rows.errors += error;
    };
    auto completed = [&](levels::LevelId const& id) {
        return profiles::summarize_level_progress(id, records).state() ==
               profiles::LevelProgressState::Completed;
    };
    rows.listings.reserve(catalog.levels.size());
    for (auto const& entry : catalog.levels) {
        auto& row{rows.listings.emplace_back()};
        row.path = path_to_utf8(entry.source_path);
        row.script = entry.source_text;
        if (!entry.definition) {
            row.error = levels::format_diagnostics(entry.definition.error());
            append_error(row.error);
            continue;
        }

        auto const& definition{*entry.definition};
        row.metadata = &definition.metadata();
        row.category = levels::authoring::catalog_category(definition);
        row.team_count = levels::participating_teams(definition).size();
        row.entity_count = definition.entities().size();
        row.has_player = !definition.player_entity_id().empty();
        row.progress = profiles::summarize_level_progress(row.metadata->id, records);
        row.indicator = profiles::level_completion_indicator_state(row.progress,
                                                                   row.metadata->par_time_seconds);
        row.prerequisites = profiles::evaluate_unlock(definition, completed);
        row.unlocked = row.prerequisites.unlocked(unlock_all);
    }
    for (auto const& entry : catalog.campaigns) {
        if (entry.definition) {
            rows.campaigns.push_back(&*entry.definition);
        } else {
            append_error(levels::format_diagnostics(entry.definition.error()));
        }
    }
    return rows;
}
}
