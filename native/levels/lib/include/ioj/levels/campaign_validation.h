#pragma once

#include <ioj/levels/campaign_definition.h>
#include <ioj/levels/diagnostics.h>

namespace ioj::levels {
[[nodiscard]] auto validate_campaign(CampaignDefinition const& definition)
    -> std::expected<void, Diagnostics>;
}
