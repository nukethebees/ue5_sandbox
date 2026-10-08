#pragma once
#include <ioj/levels/campaign_definition.h>
#include <ioj/s7/ast.h>

namespace ioj::levels::authoring {
using CampaignDefinitionReadResult = std::expected<CampaignDefinition, Diagnostics>;
// Parse structure and types from a valid node index in ast; validate before publishing.
[[nodiscard]] auto parse_campaign(s7::Ast const& ast, s7::NodeIndex root)
    -> CampaignDefinitionReadResult;
}
