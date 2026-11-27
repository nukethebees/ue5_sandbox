#pragma once

#include <ioj/presentation/mission_catalog.h>
#include <ioj/profiles/score_record.h>

#include <span>
#include <string>
#include <vector>

namespace ioj::levels::authoring {
struct DefinitionCatalog;
}

namespace ioj::bridge {
// The listing metadata, script text and campaigns borrow from the definition catalog.
struct CatalogRows {
    std::vector<presentation::MissionListing> listings;
    std::vector<levels::CampaignDefinition const*> campaigns;
    std::string errors;
};

auto make_catalog_rows(levels::authoring::DefinitionCatalog const& catalog,
                       std::span<profiles::ScoreRecord const> records,
                       bool unlock_all) -> CatalogRows;
}
