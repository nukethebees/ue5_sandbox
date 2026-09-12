#pragma once

#include <compare>
#include <cstdint>

struct FRegistryEntityHandle {
    using index_type = std::int32_t;
    using generation_type = std::int32_t;

    static constexpr index_type INDEX_NONE{-1};

    FRegistryEntityHandle() = default;
    constexpr FRegistryEntityHandle(index_type const index, generation_type const generation)
        : index(index)
        , generation(generation) {}

    auto operator<=>(FRegistryEntityHandle const&) const noexcept = default;

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return index >= 0 && generation >= 0;
    }
    [[nodiscard]] constexpr auto is_null() const noexcept -> bool {
        return index == INDEX_NONE && generation == INDEX_NONE;
    }
    constexpr void reset() noexcept {
        index = INDEX_NONE;
        generation = INDEX_NONE;
    }

    index_type index{INDEX_NONE};
    generation_type generation{INDEX_NONE};
};
