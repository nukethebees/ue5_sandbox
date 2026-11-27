#pragma once

#include <ioj/levels/campaign_definition.h>
#include <ioj/levels/level_catalog_category.h>
#include <ioj/levels/level_metadata.h>
#include <ioj/profiles/completion_indicator_state.h>
#include <ioj/profiles/level_progress_summary.h>
#include <ioj/profiles/unlock_status.h>

#include <span>
#include <string>
#include <string_view>

namespace ioj::presentation {
struct LevelListing {
    std::string path;
    std::string_view script;
    std::string error;
    levels::LevelMetadata const* metadata{};
    levels::authoring::LevelCatalogCategory category{};
    std::size_t team_count{};
    std::size_t entity_count{};
    bool has_player{};
    profiles::LevelProgressSummary progress{};
    profiles::CompletionIndicatorState indicator{};
    profiles::UnlockStatus prerequisites{};
    bool unlocked{};
};
struct LevelCatalogView {
    std::string_view directory;
    std::string_view error;
    std::span<LevelListing const> levels;
    std::span<levels::CampaignDefinition const* const> campaigns;
};
}
