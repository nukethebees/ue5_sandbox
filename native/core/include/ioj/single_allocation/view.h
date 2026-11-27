#pragma once

#include <ioj/single_allocation/column_types.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace ioj::soa_storage_detail {

inline auto checked_row_count(std::size_t const count) -> std::uint32_t {
    assert(count <= std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(count);
}

template <typename... Columns>
void validate_column_pointers([[maybe_unused]] std::uint32_t const count,
                              [[maybe_unused]] Columns* const... columns) {
    assert(count == 0 || ((columns != nullptr) && ...));
}

template <typename... Columns>
void validate_column_sizes([[maybe_unused]] std::uint32_t const count,
                           [[maybe_unused]] Columns const&... columns) {
    assert(((columns.size() == count) && ...));
}

// Views borrow their owner and range. The owner must outlive them; reallocation, owner moves,
// reset, and removal/reordering of borrowed rows invalidate them. Reacquire after invalidation.
// Accessors trust this contract; they do not detect stale borrows.

template <typename Column, typename T>
void assign_column(Column column, std::uint32_t index, T const& value) {
    if constexpr (requires { column.set(index, value); }) {
        column.set(index, value);
    } else {
        column[index] = value;
    }
}

template <typename View, typename... Ts>
void assign_row(View const& view, std::uint32_t index, Ts const&... values) {
    using Schema = typename View::soa_schema;
    assert(index < view.num());
    if constexpr (std::is_same_v<ColumnTypes<Ts...>, typename Schema::column_types>) {
        auto const& resolved{view.resolve()};
        auto const& [... columns]{resolved.physical_pointers()};
        ((columns[index] = values), ...);
    } else if constexpr (std::is_same_v<ColumnTypes<Ts...>,
                                        ColumnTypes<typename Schema::writable_equivalent>>) {
        Schema::equivalent_values(
            values..., [&](auto const&... fields) { assign_row(view, index, fields...); });
    } else {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (assign_column(Schema::columns::template logical_column<I>(view), index, values), ...);
        }(std::index_sequence_for<Ts...>{});
    }
}

template <typename Self,
          typename Base,
          typename Physical = typename Base::soa_schema::columns::physical,
          typename Logical = typename Base::soa_schema::columns::logical,
          typename Equivalent = typename Base::soa_schema::writable_equivalent>
struct ViewRowOperations;

template <typename Self, typename Base, typename... Ts, typename... Logical, typename Equivalent>
struct ViewRowOperations<Self, Base, ColumnTypes<Ts...>, ColumnTypes<Logical...>, Equivalent>
    : Base {
    using Base::Base;
    using implementation_type = ViewRowOperations;
    static constexpr bool generated_set{true};
    void set(std::uint32_t index, Ts... values) const {
        assign_row(static_cast<Self const&>(*this), index, values...);
    }
    void set(std::uint32_t index, Logical... values) const
        requires (!std::is_same_v<ColumnTypes<Ts...>, ColumnTypes<Logical...>>)
    {
        assign_row(static_cast<Self const&>(*this), index, values...);
    }
    void set(std::uint32_t index, Equivalent value) const
        requires (!std::is_same_v<Equivalent, NoEquivalentRow>)
    {
        assign_row(static_cast<Self const&>(*this), index, value);
    }
};

template <typename Schema>
struct ViewOperations : Schema {
    template <typename Self, typename Func>
    void each_column(this Self const& self, Func&& func) {
        auto const& resolved{self.resolve()};
        auto const count{self.num()};
        template for (auto* column : resolved.physical_pointers()) {
            func(std::span{column, count});
        }
    }
    auto is_empty(this auto const& self) noexcept -> bool { return self.num() == 0; }
    auto view(this auto const& self) { return self; }
    template <typename Self>
    auto const_view(this Self const& self) -> typename Self::const_view_type {
        return self;
    }
    template <typename Self>
    auto view(this Self const& self,
              typename Self::size_type offset,
              typename Self::size_type count) -> Self {
        return self.slice(offset, count);
    }
    template <typename Self>
    auto const_view(this Self const& self,
                    typename Self::size_type offset,
                    typename Self::size_type count) -> typename Self::const_view_type {
        return self.slice(offset, count);
    }
    template <typename Self>
    auto left(this Self const& self, typename Self::size_type count) -> Self {
        return self.slice(0, count);
    }
    template <typename Self>
    auto right(this Self const& self, typename Self::size_type count) -> Self {
        assert(count <= self.num());
        return self.slice(self.num() - count, count);
    }
};

// A temporary view still borrows live storage; a temporary owner destroys that storage.
template <typename T>
concept ColumnReceiver = std::is_lvalue_reference_v<T> || requires { typename T::const_view_type; };

template <std::size_t Index, typename Source>
auto column_at(Source& source) {
    return Source::soa_schema::columns::template column<Index>(source);
}

template <class View, class ConstView, class Compact, class ConstCompact, class Columns>
struct ViewTypes {
    using columns = Columns;
    using pointer_types = typename Columns::pointers;
    using column_types = typename Columns::physical;
    using writable_equivalent = NoEquivalentRow;
    static constexpr bool generated_set{false};
    static constexpr bool has_strided_columns{pointer_types::has_strided_columns};
    template <bool Const>
    using resolved_view_for = std::conditional_t<Const, ConstView, View>;
    template <bool Const>
    using compact_view_for = std::conditional_t<Const, ConstCompact, Compact>;
};

template <typename Child, bool Const, std::size_t First, typename Self>
auto nested_resolved_view(Self const& self) {
    using Result = typename Child::template resolved_view_for<Const>;
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return Result{self.template column_at<First + I>().data()..., self.num()};
    }(std::make_index_sequence<Child::pointer_types::size>{});
}

template <typename Columns, bool Const, typename Schema>
struct ViewBase;

template <bool Const, typename Schema, typename... Ts>
struct ViewBase<ColumnTypes<Ts...>, Const, Schema> : ViewOperations<Schema> {
    using size_type = std::uint32_t;
    template <typename T>
    using Element = std::conditional_t<Const, T const, T>;
    using PointerStorage = ColumnPointers<Element<Ts>...>;

    ViewBase() = default;
    ViewBase(PointerStorage columns, size_type count)
        : columns_{columns}
        , column_count_{count} {
        auto const& [... pointers]{columns};
        validate_column_pointers(count, pointers...);
    }
    ViewBase(Element<Ts>*... pointers, size_type column_count)
        : ViewBase{PointerStorage{pointers...}, column_count} {}
    ViewBase(std::span<Element<Ts>>... columns)
        : ViewBase{columns.data()..., [](auto const& first, auto const&...) {
                       return checked_row_count(first.size());
                   }(columns...)} {
        validate_column_sizes(column_count_, columns...);
    }

    auto num() const noexcept -> size_type { return column_count_; }
    auto resolve(this auto const& self) { return self; }
    auto physical_pointers() const noexcept -> PointerStorage const& { return columns_; }

    template <typename Self>
    auto slice(this Self const& self, size_type offset, size_type count) -> Self {
        assert(offset <= self.column_count_ && count <= self.column_count_ - offset);
        return Self{offset_columns(self.columns_, offset), count};
    }
    template <std::size_t Index>
    auto column_at() const {
        return std::span{columns_.template get<Index>(), column_count_};
    }

    template <typename Child, std::size_t First>
    auto nested_at(this auto const& self) {
        return nested_resolved_view<Child, Const, First>(self);
    }
  protected:
    template <typename, bool, typename>
    friend struct ViewBase;

    template <bool Enabled = Const>
        requires Enabled
    ViewBase(ViewBase<ColumnTypes<Ts...>, false, Schema> const& other)
        : columns_{convert_pointers<PointerStorage>(other.columns_)}
        , column_count_{other.column_count_} {}

    PointerStorage columns_{};
    size_type column_count_{};
};

template <typename Schema, bool Const>
struct ResolvedView : ViewBase<typename Schema::column_types, Const, Schema> {
    using implementation_type = ResolvedView;
    using Base = ViewBase<typename Schema::column_types, Const, Schema>;
    using Base::Base;
    using soa_schema = Schema;
    using view_type = typename Schema::template resolved_view_for<false>;
    using const_view_type = typename Schema::template resolved_view_for<true>;

    template <bool Enabled = Const>
        requires Enabled
    ResolvedView(ResolvedView<Schema, false> const& other)
        : Base{other} {}
};
} // namespace ioj::soa_storage_detail
