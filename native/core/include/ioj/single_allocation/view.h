#pragma once

#include <ioj/single_allocation/layout.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
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

template <typename Source>
    requires (std::is_pointer_v<std::remove_cvref_t<Source>> ||
              requires(Source const& value) { value.data(); })
constexpr auto source_data(Source const& source) noexcept {
    if constexpr (std::is_pointer_v<std::remove_cvref_t<Source>>) {
        return source;
    } else {
        return source.data();
    }
}

struct StorageState {
    std::byte* data_{};
    std::uint32_t num_{};
    std::uint32_t capacity_{};
};

// External sources may validate independent columns; generated resolved views validate on
// construction.
template <typename Source>
void validate_source(Source const& source) {
    if constexpr (requires { source.validate(); }) {
        source.validate();
    }
}

struct ViewOperations {
    template <typename Self, typename Func>
    void each_column(this Self const& self, Func&& func) {
        Self::soa_schema::with_columns(
            self, [&](auto const&... columns) { (func(std::span{columns}), ...); });
    }
    auto is_empty(this auto const& self) noexcept -> bool { return self.num() == 0; }
    auto get_view(this auto const& self) { return self; }
    template <typename Self>
    auto get_const_view(this Self const& self) -> typename Self::const_view_type {
        return self;
    }
    template <typename Self>
    auto get_view(this Self const& self,
                  typename Self::size_type offset,
                  typename Self::size_type count) -> Self {
        return self.slice(offset, count);
    }
    template <typename Self>
    auto get_const_view(this Self const& self,
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

template <typename View>
consteval auto validate_compact_view() -> bool {
    static_assert(sizeof(View) == sizeof(StorageState),
                  "Compact view must contain only a state pointer, offset, and count.");
    static_assert(std::is_trivially_copyable_v<View>,
                  "Compact view must remain trivially copyable.");
    return true;
}

inline void validate_view([[maybe_unused]] StorageState const* state,
                          [[maybe_unused]] std::uint32_t offset,
                          [[maybe_unused]] std::uint32_t count) {
    assert(state ? offset <= state->num_ && count <= state->num_ - offset
                 : offset == 0 && count == 0);
}

inline auto view_capacity_blocks(StorageState const* state) -> std::size_t {
    return state ? static_cast<std::size_t>(state->capacity_ /
                                            single_allocation_layout::capacity_granularity)
                 : 0;
}

template <typename T, typename State>
auto view_column_data_unchecked(State* state, std::uint32_t offset, std::size_t byte_offset)
    -> std::conditional_t<std::is_const_v<State>, T const, T>* {
    using Element = std::conditional_t<std::is_const_v<State>, T const, T>;
    return std::launder(reinterpret_cast<Element*>(state->data_ + byte_offset)) + offset;
}

template <typename T, typename State>
auto view_column_data(State* state, std::uint32_t offset, std::size_t byte_offset)
    -> std::conditional_t<std::is_const_v<State>, T const, T>* {
    if (!state || !state->data_) {
        return nullptr;
    }
    return view_column_data_unchecked<T>(state, offset, byte_offset);
}

template <bool Const>
struct CompactViewBase : ViewOperations {
    using size_type = std::uint32_t;
    using Storage = StorageState;
    using State = std::conditional_t<Const, Storage const, Storage>;
    template <typename T>
    using Element = std::conditional_t<Const, T const, T>;

    CompactViewBase() = default;
    CompactViewBase(State* state, size_type offset, size_type count)
        : state_{state}
        , offset_{offset}
        , count_{count} {}

    void validate(this auto const& self) { validate_view(self.state_, self.offset_, self.count_); }
    auto num(this auto const& self) noexcept { return self.count_; }
    template <typename Self>
    auto resolve(this Self const& self) -> typename Self::resolved_view_type {
        single_allocation_layout::LayoutCursor cursor{view_capacity_blocks(self.state_)};
        return Self::Layout::template make_resolved_view<typename Self::resolved_view_type>(
            [&]<typename T> {
                return view_column_data<T>(
                    self.state_, self.offset_, cursor.advance(sizeof(T), alignof(T)));
            },
            self.count_);
    }
    template <typename Self>
    auto slice(this Self const& self,
               typename Self::size_type offset,
               typename Self::size_type count) -> Self {
        assert(offset <= self.count_ && count <= self.count_ - offset);
        return Self{self.state_, self.offset_ + offset, count};
    }
  protected:
    template <bool>
    friend struct CompactViewBase;

    template <bool Enabled = Const>
        requires Enabled
    CompactViewBase(CompactViewBase<false> const& other)
        : state_{other.state_}
        , offset_{other.offset_}
        , count_{other.count_} {}

    template <typename T, typename Self>
    auto column(this Self const& self, std::size_t index) {
        return std::span<Element<T>>{
            view_column_data<T>(self.state_,
                                self.offset_,
                                Self::Layout::offset(index, view_capacity_blocks(self.state_))),
            self.count_};
    }

    State* state_{};
    size_type offset_{};
    size_type count_{};
};

template <typename T, bool Const>
struct StridedViewBase : ViewOperations {
    using size_type = std::uint32_t;
    template <typename U>
    using Element = std::conditional_t<Const, U const, U>;
    using element_type = Element<T>;

    StridedViewBase() = default;

    // All columns must belong to one allocation and have the same byte stride. Each must
    // contain count live elements; independently allocated columns require a resolved view.
    StridedViewBase(element_type* first, std::size_t byte_stride, size_type count)
        : first_{reinterpret_cast<Element<std::byte>*>(first)}
        , stride_{encode_stride(byte_stride)}
        , count_{count} {
        validate_column_pointers(count, first);
        assert(byte_stride % alignof(T) == 0);
    }

    auto num() const noexcept -> size_type { return count_; }

    template <typename Self>
    auto resolve(this Self const& self) -> typename Self::resolved_view_type {
        std::size_t index{};
        return Self::Layout::template make_resolved_view<typename Self::resolved_view_type>(
            [&]<typename> { return self.column(index++).data(); }, self.num());
    }

    template <typename Self>
    auto slice(this Self const& self, size_type offset, size_type count) -> Self {
        assert(offset <= self.count_ && count <= self.count_ - offset);
        auto result{self};
        if (result.first_) {
            result.first_ += static_cast<std::size_t>(offset) * sizeof(T);
        }
        result.count_ = count;
        return result;
    }
  protected:
    template <typename, bool>
    friend struct StridedViewBase;

    template <bool Enabled = Const>
        requires Enabled
    StridedViewBase(StridedViewBase<T, false> const& other)
        : first_{other.first_}
        , stride_{other.stride_}
        , count_{other.count_} {}

    auto column(std::size_t index) const -> std::span<element_type> {
        auto* data{first_ ? reinterpret_cast<element_type*>(first_ + index * byte_stride())
                          : nullptr};
        return {data, count_};
    }
  private:
    using Policy = single_allocation_layout::LayoutPolicy;
    static constexpr auto alignment{
        alignof(T) > Policy::minimum_alignment ? alignof(T) : Policy::minimum_alignment};
    static constexpr auto gap{
        single_allocation_layout::layout_align(Policy::column_gap, alignment)};
    // Rounded capacities leave one granularity block below the uint32 limit. Store the
    // stride in elements only when the fixed gap fits in that remaining space exactly.
    static constexpr bool compact_stride{gap % sizeof(T) == 0 &&
                                         gap / sizeof(T) < Policy::capacity_granularity};
    using Stride = std::conditional_t<compact_stride, size_type, std::size_t>;

    static auto encode_stride(std::size_t bytes) -> Stride {
        if constexpr (compact_stride) {
            assert(bytes % sizeof(T) == 0);
            assert(bytes / sizeof(T) <= std::numeric_limits<size_type>::max());
            return static_cast<size_type>(bytes / sizeof(T));
        } else {
            return bytes;
        }
    }
  protected:
    auto byte_stride() const noexcept -> std::size_t {
        if constexpr (compact_stride) {
            return static_cast<std::size_t>(stride_) * sizeof(T);
        } else {
            return stride_;
        }
    }
  private:
    Element<std::byte>* first_{};
    Stride stride_{};
    size_type count_{};
};

template <typename View, typename Layout, typename State>
auto make_strided_view(
    State* state, std::uint32_t offset, std::uint32_t count, std::size_t first, std::size_t second)
    -> View {
    using T = typename View::element_type;
    validate_view(state, offset, count);
    if (!state || !state->data_) {
        return {};
    }
    auto const blocks{view_capacity_blocks(state)};
    auto const first_offset{Layout::offset(first, blocks)};
    auto const stride{Layout::offset(second, blocks) - first_offset};
    return View{view_column_data_unchecked<T>(state, offset, first_offset), stride, count};
}

} // namespace ioj::soa_storage_detail
