#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/levels/identifiers.h>

#include <string>
#include <vector>

namespace ioj::levels {
struct CampaignDefinition {
    CampaignId id{};
    std::string title{};
    std::vector<LevelId> level_ids{};
};
[[nodiscard]] auto validate_campaign(CampaignDefinition const& definition)
    -> std::expected<void, Diagnostics>;
} // namespace ioj::levels
