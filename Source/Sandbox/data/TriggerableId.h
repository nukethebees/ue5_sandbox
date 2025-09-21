#pragma once

#include <array>
#include <bit>

#include "CoreMinimal.h"

struct TriggerableId {
  public:
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

    auto as_combined_id() const { return std::bit_cast<uint64>(indexes_); }

    bool is_valid() const { return tuple_index() >= 0 && array_index() >= 0; }

    static TriggerableId invalid() { return {}; }
  private:
    std::array<int32, 2> indexes_{-1, -1};
};
