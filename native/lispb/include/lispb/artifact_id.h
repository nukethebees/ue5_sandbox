#pragma once

#include <lispb/artifact_kind.h>

#include <cstdint>
namespace lispb {

struct ArtifactId {
    std::uint32_t value{};

    auto kind() const -> ArtifactKind { return static_cast<ArtifactKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffff; }
    auto operator==(ArtifactId const&) const -> bool = default;
};

}
