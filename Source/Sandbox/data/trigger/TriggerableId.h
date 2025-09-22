#pragma once

#include <array>
#include <bit>
#include <compare>

#include "CoreMinimal.h"

struct TriggerableId {
  public:
    using CombinedId = uint64;

    TriggerableId() = default;
    TriggerableId(int32 tuple_idx, int32 array_idx)
        : indexes_({tuple_idx, array_idx}) {}

    template <typename Self>
    auto&& tuple_index(this Self&& self) {
        return std::forward<Self>(self).indexes_[0];
    }

    template <typename Self>
    auto&& array_index(this Self&& self) {
        return std::forward<Self>(self).indexes_[1];
    }

    auto as_combined_id() const -> CombinedId { return std::bit_cast<CombinedId>(indexes_); }

    bool is_valid() const { return tuple_index() >= 0 && array_index() >= 0; }

    static TriggerableId invalid() { return {}; }

    static TriggerableId from_combined_id(CombinedId combined_id) {
        auto indexes{std::bit_cast<std::array<int32, 2>>(combined_id)};
        return TriggerableId{indexes[0], indexes[1]};
    }

    auto operator<=>(TriggerableId const& other) const = default;
  private:
    std::array<int32, 2> indexes_{-1, -1};
};
