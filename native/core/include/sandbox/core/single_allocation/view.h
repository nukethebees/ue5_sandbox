#pragma once

#include <sandbox/core/single_allocation/layout.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace ml::soa_storage_detail {

// Views borrow their owner and range. The owner must outlive them; reallocation, owner moves,
// reset, and removal/reordering of borrowed rows invalidate them. Reacquire after invalidation.
// Accessors trust this contract; they do not detect stale borrows.

template <typename Source>
    requires (
        std::is_pointer_v<std::remove_cvref_t<Source>> ||
        requires(Source const& value) { value.data(); } ||
        requires(Source const& value) { value.GetData(); })
constexpr auto source_data(Source const& source) noexcept {
    if constexpr (std::is_pointer_v<std::remove_cvref_t<Source>>) {
        return source;
    } else if constexpr (requires { source.data(); }) {
        return source.data();
    } else {
        return source.GetData();
    }
}

template <typename Size>
struct StorageState {
    std::byte* data_{};
    Size num_{};
    Size capacity_{};
};

template <typename View>
consteval auto validate_compact_view() -> bool {
    static_assert(sizeof(View) == sizeof(StorageState<std::uint32_t>),
                  "Compact view must contain only a state pointer, offset, and count.");
    static_assert(std::is_trivially_copyable_v<View>,
                  "Compact view must remain trivially copyable.");
    return true;
}

template <typename Size>
void validate_view([[maybe_unused]] StorageState<Size> const* state,
                   [[maybe_unused]] Size offset,
                   [[maybe_unused]] Size count) {
    assert(offset >= 0 && count >= 0);
    assert(state ? offset <= state->num_ && count <= state->num_ - offset
                 : offset == 0 && count == 0);
}

// Keep the borrowed state in each named generated view; share only its operations.
struct CompactViewOperations {
    void validate(this auto const& self) { validate_view(self.state_, self.offset_, self.count_); }
    auto num(this auto const& self) noexcept { return self.count_; }
    auto is_empty(this auto const& self) noexcept -> bool { return self.count_ == 0; }
    auto get_view(this auto const& self) { return self; }
    template <typename Self>
    auto get_const_view(this Self const& self) -> typename Self::ConstView {
        return self;
    }
    template <typename Self>
    auto slice(this Self const& self,
               typename Self::size_type offset,
               typename Self::size_type count) -> Self {
        assert(offset >= 0 && offset <= self.count_ && count >= 0 && count <= self.count_ - offset);
        return Self{self.state_, self.offset_ + offset, count};
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
                        typename Self::size_type count) -> typename Self::ConstView {
        return self.slice(offset, count);
    }
    template <typename Self>
    auto left(this Self const& self, typename Self::size_type count) -> Self {
        return self.slice(0, count);
    }
    template <typename Self>
    auto right(this Self const& self, typename Self::size_type count) -> Self {
        assert(count >= 0 && count <= self.count_);
        return self.slice(self.count_ - count, count);
    }
};

template <typename Size>
auto view_capacity_blocks(StorageState<Size> const* state) -> std::size_t {
    return state ? static_cast<std::size_t>(state->capacity_ /
                                            single_allocation_layout::capacity_granularity)
                 : 0;
}

template <typename T, typename State, typename Size>
auto view_column_data_unchecked(State* state, Size offset, std::size_t byte_offset)
    -> std::conditional_t<std::is_const_v<State>, T const, T>* {
    using Element = std::conditional_t<std::is_const_v<State>, T const, T>;
    return std::launder(reinterpret_cast<Element*>(state->data_ + byte_offset)) + offset;
}

template <typename T, typename State, typename Size>
auto view_column_data(State* state, Size offset, std::size_t byte_offset)
    -> std::conditional_t<std::is_const_v<State>, T const, T>* {
    if (!state || !state->data_) {
        return nullptr;
    }
    return view_column_data_unchecked<T>(state, offset, byte_offset);
}

template <typename View, typename State, typename Size, typename T>
auto column_view(State* state,
                 Size offset,
                 Size count,
                 single_allocation_layout::ColumnLayout<T> const& column) -> View {
    return {view_column_data<T>(state, offset, column.offset(view_capacity_blocks(state))), count};
}

template <typename View, typename State, typename Size, typename T>
auto three_column_view(State* state,
                       Size offset,
                       Size count,
                       single_allocation_layout::ColumnLayout<T> const& first,
                       single_allocation_layout::ColumnLayout<T> const& second,
                       single_allocation_layout::ColumnLayout<T> const& third) -> View {
    if (!state || !state->data_) {
        return {};
    }
    auto const blocks{view_capacity_blocks(state)};
    return {view_column_data_unchecked<T>(state, offset, first.offset(blocks)),
            view_column_data_unchecked<T>(state, offset, second.offset(blocks)),
            view_column_data_unchecked<T>(state, offset, third.offset(blocks)),
            count};
}

template <typename View, typename State, typename Size, typename T>
auto strided_vector_view(State* state,
                         Size offset,
                         Size count,
                         single_allocation_layout::ColumnLayout<T> const& first,
                         single_allocation_layout::ColumnLayout<T> const& second) -> View {
    if (!state || !state->data_) {
        return {};
    }
    auto const blocks{view_capacity_blocks(state)};
    auto const first_offset{first.offset(blocks)};
    return {view_column_data_unchecked<T>(state, offset, first_offset),
            second.offset(blocks) - first_offset,
            count};
}

}
