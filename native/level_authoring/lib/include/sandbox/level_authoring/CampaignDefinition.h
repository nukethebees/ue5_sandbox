#pragma once

#include <string>
#include <vector>

namespace ml::level_authoring {
struct CampaignDefinition {
    std::string id{};
    std::string title{};
    std::vector<std::string> level_ids{};
};
} // namespace ml::level_authoring
