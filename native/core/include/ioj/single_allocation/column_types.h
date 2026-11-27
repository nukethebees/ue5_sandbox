#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace ioj::soa_storage_detail {
// All columns in a resolved view have the same access mode. Erase only the element
// type in storage, retaining constness and one address per column.
template <typename... Ts>
struct ColumnPointers {
    template <std::size_t Index>
    using element_type = Ts...[Index];
    template <template <typename...> typename Result>
    using apply = Result<Ts...>;
    static constexpr std::size_t size{sizeof...(Ts)};
    static constexpr bool has_strided_columns{size >= 2 &&
                                              (std::is_same_v<Ts, element_type<0>> && ...)};
    using Address = std::conditional_t<(std::is_const_v<Ts> && ...), void const*, void*>;

    ColumnPointers() = default;
    explicit ColumnPointers(Ts*... columns) noexcept
        : columns_{columns...} {}

    template <std::size_t Index>
    auto get() const noexcept -> element_type<Index>* {
        return static_cast<element_type<Index>*>(columns_[Index]);
    }

  private:
    std::array<Address, sizeof...(Ts)> columns_{};
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
struct ColumnTypes {};

template <typename Result>
auto convert_pointers(auto const& data) noexcept -> Result {
    auto const& [... columns]{data};
    return Result{columns...};
}

template <typename Data>
auto offset_columns(Data const& pointers, std::uint32_t offset) noexcept -> Data {
    auto const& [... columns]{pointers};
    return Data{(columns ? columns + offset : nullptr)...};
}
}
