#pragma once

#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/soa_permutation.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ioj::sim {
struct EntityTableOperations {
    // Views use EntityInstanceHandle::index() within the selected entity type.
    template <EntityType Type, typename Self>
        requires (!std::is_const_v<Self>)
    [[nodiscard]] auto get_view(this Self& self, std::size_t const count) {
        static_assert(Self::is_supported_type(Type));
        return self.get_view(Type, count);
    }

    template <typename Self>
        requires (!std::is_const_v<Self>)
    [[nodiscard]] auto get_view(this Self& self, EntityType const type, std::size_t const count) {
        return make_view_impl<typename Self::view_type>(self, type, count);
    }

    template <EntityType Type, typename Self>
    [[nodiscard]] auto get_const_view(this Self const& self, std::size_t const count) {
        static_assert(Self::is_supported_type(Type));
        return self.get_const_view(Type, count);
    }

    template <typename Self>
    [[nodiscard]] auto
        get_const_view(this Self const& self, EntityType const type, std::size_t const count) {
        return make_view_impl<typename Self::const_view_type>(self, type, count);
    }

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

  private:
    template <typename View, typename Self>
    [[nodiscard]] static auto
        make_view_impl(Self& self, EntityType const type, std::size_t const count) -> View {
        assert(Self::is_supported_type(type));
        return self.with_columns(type, count, [](auto... columns) { return View{columns...}; });
    }
};
}
