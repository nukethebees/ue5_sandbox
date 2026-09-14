#pragma once

#include <ioj/sim/levels/level_definition.h>
#include <sandbox/level_authoring/CampaignDefinition.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ml::level_authoring {
template <typename Definition>
struct CatalogEntry {
    std::string filename{};
    std::optional<Definition> definition{};
};

struct CatalogValidationIssue {
    std::size_t entry_index{};
    std::string message{};
};

using LevelCatalogEntry = CatalogEntry<::ioj::sim::levels::LevelDefinition>;
using CampaignCatalogEntry = CatalogEntry<CampaignDefinition>;

[[nodiscard]] auto validate_level_catalog(std::vector<LevelCatalogEntry> const& entries)
    -> std::vector<CatalogValidationIssue>;
[[nodiscard]] auto
    validate_campaign_catalog(std::vector<CampaignCatalogEntry> const& campaigns,
                              std::vector<LevelCatalogEntry> const& levels,
                              std::vector<CatalogValidationIssue> const& level_issues)
        -> std::vector<CatalogValidationIssue>;
} // namespace ml::level_authoring
