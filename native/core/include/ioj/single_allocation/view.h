#pragma once

#include <ioj/single_allocation/layout.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
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
        assert(offset <= self.count_ && count <= self.count_ - offset);
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
        assert(count <= self.count_);
        return self.slice(self.count_ - count, count);
    }
};

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

template <typename View, typename State, typename T>
auto column_view(State* state,
                 std::uint32_t offset,
                 std::uint32_t count,
                 single_allocation_layout::ColumnLayout<T> const& column) -> View {
    return {view_column_data<T>(state, offset, column.offset(view_capacity_blocks(state))), count};
}

} // namespace ioj::soa_storage_detail
