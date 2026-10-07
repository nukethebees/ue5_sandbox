#include <ioj/levels/campaign_definition.h>

#include <ioj/ascii.h>

#include <format>
#include <unordered_set>

namespace ioj::levels {
auto validate_campaign(CampaignDefinition const& definition) -> std::expected<void, Diagnostics> {
    Diagnostics errors;
    if (definition.id.empty()) {
        errors.push_back(
            {DiagnosticCode::MissingCampaignId, "campaign.id", "Campaign has no stable id"});
    }
    if (ioj::blank(definition.title)) {
        errors.push_back({DiagnosticCode::MissingTitle, "campaign.title", "Campaign has no title"});
    }
    if (definition.level_ids.empty()) {
        errors.push_back(
            {DiagnosticCode::MissingCampaignLevels, "campaign.levels", "Campaign has no levels"});
    }
    std::unordered_set<LevelId> seen;
    auto const count{definition.level_ids.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& id{definition.level_ids[index]};
        if (id.empty() || !seen.insert(id).second) {
            errors.push_back({id.empty() ? DiagnosticCode::MissingLevelId
                                         : DiagnosticCode::DuplicateCampaignLevel,
                              std::format("campaign.levels[{}]", index),
                              std::format("Level id '{}' is empty or duplicated", id.value)});
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}
}
