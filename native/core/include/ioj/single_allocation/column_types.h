#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ioj::soa_storage_detail {
// Each level contains its pointers as members, preserving standard layout for borrowed views.
template <typename T, typename... Rest>
struct ColumnPointers {
    ColumnPointers() = default;
    explicit ColumnPointers(T* first, Rest*... rest) noexcept
        : first_{first}
        , rest_{rest...} {}

    template <std::size_t Index>
    auto get(this auto&& self) noexcept -> decltype(auto) {
        if constexpr (Index == 0) {
            return std::forward_like<decltype(self)>(self.first_);
        } else {
            return std::forward_like<decltype(self)>(self.rest_).template get<Index - 1>();
        }
    }

  private:
    T* first_{};
    ColumnPointers<Rest...> rest_{};
};

template <typename T>
struct ColumnPointers<T> {
    ColumnPointers() = default;
    explicit ColumnPointers(T* pointer) noexcept
        : pointer_{pointer} {}

    template <std::size_t Index>
        requires (Index == 0)
    auto get(this auto&& self) noexcept -> decltype(auto) {
        return std::forward_like<decltype(self)>(self.pointer_);
    }

  private:
    T* pointer_{};
};
}

namespace std {
template <typename... Ts>
struct tuple_size<ioj::soa_storage_detail::ColumnPointers<Ts...>>
    : integral_constant<size_t, sizeof...(Ts)> {};

template <size_t Index, typename... Ts>
struct tuple_element<Index, ioj::soa_storage_detail::ColumnPointers<Ts...>> {
    using type = Ts...[Index]*;
};
}

namespace ioj::soa_storage_detail {
template <typename... Ts>
struct ColumnTypes {
    template <std::size_t Index>
    using at = Ts...[Index];
};

template <template <typename...> typename Result, typename Pointers>
using FromPointers = typename decltype([] {
    auto [... columns]{Pointers{}};
    return std::type_identity<Result<std::remove_pointer_t<decltype(columns)>...>>{};
}())::type;

template <typename Result>
auto convert_pointers(auto const& data) noexcept -> Result {
    auto const& [... columns]{data};
    return Result{columns...};
}

template <typename Data>
auto offset_columns(Data pointers, std::uint32_t offset) noexcept -> Data {
    template for (auto& column : pointers) {
        if (column) {
            column += offset;
        }
    }
    return pointers;
}
}
