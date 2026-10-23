#pragma once

#include <lispb/artifact_kind.h>

#include <cstdint>
namespace lispb {

struct ArtifactId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};

    auto kind() const -> ArtifactKind { return static_cast<ArtifactKind>(value >> kind_shift); }
    auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator==(ArtifactId const&) const -> bool = default;
};

}
