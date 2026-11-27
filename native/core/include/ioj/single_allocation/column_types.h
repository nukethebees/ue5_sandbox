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
struct ColumnTypes {
    template <template <typename...> typename List>
    using apply = List<Ts...>;
    static constexpr std::size_t size{sizeof...(Ts)};
};

// Used only in decltype to concatenate the leaf types of adjacent schema members.
template <typename... Left, typename... Right>
auto operator+(ColumnTypes<Left...>, ColumnTypes<Right...>) -> ColumnTypes<Left..., Right...>;

struct NoEquivalentRow {};

template <typename Child>
struct NestedColumn {
    using schema = Child;
};

template <typename T>
struct ColumnTraits {
    static constexpr bool nested{false};
    using physical = ColumnTypes<T>;
    using logical = ColumnTypes<T>;
};

template <typename Child>
struct ColumnTraits<NestedColumn<Child>> {
    static constexpr bool nested{true};
    using physical = typename Child::columns::physical;
    using value_type = typename Child::writable_equivalent;
    using logical = std::conditional_t<std::is_same_v<value_type, NoEquivalentRow>,
                                       typename Child::columns::logical,
                                       ColumnTypes<value_type>>;
};

template <typename... Members>
struct SchemaColumns {
    using physical = decltype((ColumnTypes<>{} + ... + typename ColumnTraits<Members>::physical{}));
    using logical = decltype((ColumnTypes<>{} + ... + typename ColumnTraits<Members>::logical{}));
    using pointers = typename physical::template apply<ColumnPointers>;

    template <std::size_t Index>
    static auto column(auto& source) {
        using Member = Members...[Index];
        constexpr auto first{physical_offsets[Index]};
        if constexpr (ColumnTraits<Member>::nested) {
            return source.template nested_at<typename Member::schema, first>();
        } else {
            return source.template column_at<first>();
        }
    }

    template <std::size_t Index>
    static auto logical_column(auto const& view) {
        constexpr auto member{[] {
            std::size_t i{};
            while (logical_offsets[i + 1] <= Index) {
                ++i;
            }
            return i;
        }()};
        using Member = Members...[member];
        if constexpr (ColumnTraits<Member>::nested) {
            using Child = typename Member::schema;
            if constexpr (std::is_same_v<typename Child::writable_equivalent, NoEquivalentRow>) {
                return Child::columns::template logical_column<Index - logical_offsets[member]>(
                    column<member>(view));
            } else {
                return column<member>(view);
            }
        } else {
            return column<member>(view);
        }
    }

  private:
    static consteval auto offsets(std::array<std::size_t, sizeof...(Members)> widths) {
        std::array<std::size_t, sizeof...(Members) + 1> result{};
        for (std::size_t i{}; i < widths.size(); ++i) {
            result[i + 1] = result[i] + widths[i];
        }
        return result;
    }
    static constexpr auto physical_offsets{offsets({ColumnTraits<Members>::physical::size...})};
    static constexpr auto logical_offsets{offsets({ColumnTraits<Members>::logical::size...})};
};

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
