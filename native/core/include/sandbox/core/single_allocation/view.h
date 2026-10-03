#pragma once

#include <sandbox/core/single_allocation/layout.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace ml::soa_storage_detail {

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

template <typename Self, typename State, typename Size>
auto slice_view(State* state, Size view_offset, Size view_count, Size offset, Size count)
    -> std::remove_cvref_t<Self> {
    using View = std::remove_cvref_t<Self>;
    validate_view(state, view_offset, view_count);
    assert(offset >= 0 && offset <= view_count && count >= 0 && count <= view_count - offset);
    return View{state, view_offset + offset, count};
}

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
auto view_column_data(State* state, Size offset, Size count, std::size_t byte_offset)
    -> std::conditional_t<std::is_const_v<State>, T const, T>* {
    validate_view(state, offset, count);
    if (!state || !state->data_) {
        return nullptr;
    }
    return view_column_data_unchecked<T>(state, offset, byte_offset);
}

}
