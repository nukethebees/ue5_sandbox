#pragma once
#include <ioj/levels/campaign_definition.h>
#include <ioj/levels/level_definition.h>

#include <cstddef>
#include <filesystem>
#include <span>

namespace ioj::levels {
using CatalogEntryIndex = std::size_t;
template <typename Definition>
struct DecodedCatalogEntry {
    CatalogEntryIndex source_index{};
    std::filesystem::path source_path{};
    Definition definition{};
};
using LevelCatalogEntry = DecodedCatalogEntry<LevelDefinition>;
using CampaignCatalogEntry = DecodedCatalogEntry<CampaignDefinition>;
struct CatalogValidationIssue {
    CatalogEntryIndex entry_index{};
    Diagnostic diagnostic{};
};
// Entries contain successfully parsed and semantically validated definitions.
// Source indices must be unique; issue indices refer to discovery rows, not span positions.
[[nodiscard]] auto validate_level_catalog(std::span<LevelCatalogEntry const> entries)
    -> std::vector<CatalogValidationIssue>;
// Levels contains only entries that passed level catalog validation.
[[nodiscard]] auto validate_campaign_catalog(std::span<CampaignCatalogEntry const> campaigns,
                                             std::span<LevelCatalogEntry const> levels)
    -> std::vector<CatalogValidationIssue>;
}
