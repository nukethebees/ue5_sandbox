#pragma once

#include <optional>
#include <string>

namespace ioj::sim {
struct LevelResultMetadata {
    std::string level_id{};
    std::string level_title{};
    std::optional<float> par_time_seconds;
};
}
