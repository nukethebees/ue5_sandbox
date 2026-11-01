#pragma once

#include <ioj/sim/entities/entity_type.h>

#include <cassert>
#include <type_traits>

namespace ioj::sim {
struct EntityTableOperations {
    // Views cover lifetime slots indexed by EntityId::index() for the selected type.
    template <EntityType Type, typename Self>
        requires (!std::is_const_v<Self>)
    [[nodiscard]] auto view(this Self& self) {
        static_assert(Self::is_supported_type(Type));
        return self.view(Type);
    }

    template <typename Self>
        requires (!std::is_const_v<Self>)
    [[nodiscard]] auto view(this Self& self, EntityType const type) {
        return make_view_impl<typename Self::view_type>(self, type);
    }

    template <EntityType Type, typename Self>
    [[nodiscard]] auto const_view(this Self const& self) {
        static_assert(Self::is_supported_type(Type));
        return self.const_view(Type);
    }

    template <typename Self>
    [[nodiscard]] auto const_view(this Self const& self, EntityType const type) {
        return make_view_impl<typename Self::const_view_type>(self, type);
    }

  private:
    template <typename View, typename Self>
    [[nodiscard]] static auto make_view_impl(Self& self, EntityType const type) -> View {
        assert(Self::is_supported_type(type));
        return self.with_columns(type, [type](auto... columns) { return View{type, columns...}; });
    }
};
}
