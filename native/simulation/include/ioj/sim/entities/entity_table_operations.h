#pragma once

#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/soa_permutation.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
struct EntityTableOperations {
  protected:
    // Rows refer to the original owner order and must be strictly descending.
    void remove_rows_impl(this auto& self,
                          EntityType const type,
                          std::size_t count,
                          std::span<EntityFrameIndex::value_type const> const rows) {
        self.with_columns(type, count, [&](auto const... columns) {
            [[maybe_unused]] auto previous{count};
            for (auto const row : rows) {
                assert(row < previous && row < count);
                previous = row;
                auto const last{--count};
                ((columns[row] = columns[last]), ...);
            }
        });
    }

    void apply_permutation_impl(this auto& self,
                                EntityType const type,
                                std::span<std::int32_t> const order) {
        self.with_columns(type, order.size(), [&](auto const... columns) {
            (ioj::apply_permutation(columns, order), ...);
        });
    }
};
}
