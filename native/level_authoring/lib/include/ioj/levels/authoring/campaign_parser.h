#pragma once
#include <ioj/levels/campaign_definition.h>
#include <ioj/s7/ast.h>

namespace ioj::levels::authoring {
using CampaignDefinitionReadResult = std::expected<CampaignDefinition, Diagnostics>;
[[nodiscard]] auto parse_campaign(s7::Ast const& ast) -> CampaignDefinitionReadResult;
}
