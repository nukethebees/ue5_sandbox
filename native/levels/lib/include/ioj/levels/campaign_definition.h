#pragma once

#include <string>
#include <vector>

namespace ioj::levels {
struct CampaignDefinition {
    std::string id{};
    std::string title{};
    std::vector<std::string> level_ids{};
};
} // namespace ioj::levels
