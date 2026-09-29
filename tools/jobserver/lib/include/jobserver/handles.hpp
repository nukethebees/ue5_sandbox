#pragma once
#include <cstdint>

namespace jobserver {
struct ClientId {
    std::uint64_t value{};
    auto operator==(ClientId const&) const -> bool = default;
};
}
