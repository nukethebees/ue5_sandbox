#pragma once

#include <ioj/single_allocation/column_types.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

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

template <typename Self, typename... Ts>
void assign_row(Self& self, std::uint32_t index, Ts const&... values) {
    assert(index < self.num());
    auto const view{self.view()};
    Self::soa_schema::with_columns(view,
                                   [&](auto... columns) { ((columns[index] = values), ...); });
}

template <typename Self, typename Columns, typename Base>
struct ViewRowOperations;

template <typename Self, typename Base, typename... Ts>
struct ViewRowOperations<Self, ColumnTypes<Ts...>, Base> : Base {
    using Base::Base;
    void set(std::uint32_t index, Ts const... values) const {
        assign_row(static_cast<Self const&>(*this), index, values...);
    }
};

struct ViewOperations {
    template <typename Self, typename Func>
    void each_column(this Self const& self, Func&& func) {
        Self::soa_schema::with_columns(
            self, [&](auto const&... columns) { (func(std::span{columns}), ...); });
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

template <typename Columns, template <typename> typename Pointers, bool Const>
struct ViewBase;

template <template <typename> typename Pointers, bool Const, typename... Ts>
struct ViewBase<ColumnTypes<Ts...>, Pointers, Const> : ViewOperations {
    using size_type = std::uint32_t;
    template <typename T>
    using Element = std::conditional_t<Const, T const, T>;
    using PointerStorage = Pointers<Element<std::byte>>;

    ViewBase() = default;
    ViewBase(Element<Ts>*... pointers, size_type column_count)
        : columns_{pointers...}
        , column_count_{column_count} {
        validate_column_pointers(column_count_, pointers...);
    }
    ViewBase(std::span<Element<Ts>>... columns)
        : ViewBase{columns.data()..., [](auto const& first, auto const&...) {
                       return checked_row_count(first.size());
                   }(columns...)} {
        validate_column_sizes(column_count_, columns...);
    }

    auto num() const noexcept -> size_type { return column_count_; }
    auto resolve(this auto const& self) { return self; }

    template <typename Self>
    auto slice(this Self const& self, size_type offset, size_type count) -> Self {
        assert(offset <= self.column_count_ && count <= self.column_count_ - offset);
        return PointerStorage::with_columns(self.columns_, [&](auto*... pointers) {
            return Self{(pointers ? pointers + offset : nullptr)..., count};
        });
    }
  protected:
    template <typename, template <typename> typename, bool>
    friend struct ViewBase;

    template <bool Enabled = Const>
        requires Enabled
    ViewBase(ViewBase<ColumnTypes<Ts...>, Pointers, false> const& other)
        : column_count_{other.column_count_} {
        Pointers<std::byte>::with_columns(
            other.columns_, [this](auto*... source) { columns_ = PointerStorage{source...}; });
    }

    template <typename T>
    auto column(T* pointer) const {
        return std::span{pointer, column_count_};
    }

    PointerStorage columns_{};
    size_type column_count_{};
};

} // namespace ioj::soa_storage_detail
