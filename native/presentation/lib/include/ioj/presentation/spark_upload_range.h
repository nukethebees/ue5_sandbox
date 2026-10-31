#pragma once

#include <cstdint>

namespace ioj::presentation {
struct SparkUploadRange {
    std::int32_t destination{};
    std::int32_t source{};
    std::int32_t count{};
};
}
