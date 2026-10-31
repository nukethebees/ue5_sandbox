#pragma once

#include <cstdint>

namespace ioj::presentation {
struct EntityOverlayCollectionResult {
    std::int32_t candidate_count{};
    std::int32_t invalid_health_count{};
};
}
