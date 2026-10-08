#pragma once
#include <ioj/levels/authoring/definition_entry.h>
#include <ioj/levels/campaign_definition.h>
#include <ioj/levels/level_definition.h>

#include <vector>

namespace ioj::levels::authoring {
struct DefinitionCatalog {
    std::vector<DefinitionEntry<LevelDefinition>> levels;
    std::vector<DefinitionEntry<CampaignDefinition>> campaigns;
};
}
