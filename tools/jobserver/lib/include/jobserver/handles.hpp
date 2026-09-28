#pragma once

#include <cstdint>

namespace jobserver {
struct ClientId {
    std::uint64_t value{};
    auto operator==(ClientId const&) const -> bool = default;
};
struct CommandId {
    std::uint64_t value{};
    auto operator==(CommandId const&) const -> bool = default;
};
struct LeaseId {
    std::uint64_t value{};
    auto operator==(LeaseId const&) const -> bool = default;
};
struct GateId {
    std::uint32_t value{};
    auto operator==(GateId const&) const -> bool = default;
};
struct PayloadId {
    std::uint64_t value{};
    auto operator==(PayloadId const&) const -> bool = default;
};
}
