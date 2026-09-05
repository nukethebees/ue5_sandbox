#include "SpaceGame/levels/CampaignDefinition.h"

namespace ml {
FCampaignDefinition::FCampaignDefinition() = default;
FCampaignDefinition::FCampaignDefinition(FCampaignDefinition const&) = default;
FCampaignDefinition::FCampaignDefinition(FCampaignDefinition&&) noexcept = default;
auto FCampaignDefinition::operator=(FCampaignDefinition const&) -> FCampaignDefinition& = default;
auto FCampaignDefinition::operator=(FCampaignDefinition&&) noexcept
    -> FCampaignDefinition& = default;
FCampaignDefinition::~FCampaignDefinition() = default;
}
