#pragma once

#include <ioj/levels/identifiers.h>

#include <optional>
#include <string>

namespace ioj::levels {
struct LevelMetadata {
    LevelId id{};
    std::string title{};
    std::string description{};
    std::optional<float> par_time_seconds{};
};
}
