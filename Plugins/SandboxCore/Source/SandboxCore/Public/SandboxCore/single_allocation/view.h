#pragma once

#include <SandboxCore/single_allocation/layout.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace ml::soa_storage_detail {

struct StorageState {
    std::byte* data_{};
    std::int32_t num_{};
    std::int32_t capacity_{};
};

template <bool Const, auto Require>
struct CompactViewState {
    using size_type = std::int32_t;
    using State = std::conditional_t<Const, StorageState const, StorageState>;
    template <typename T>
    using Element = std::conditional_t<Const, T const, T>;

    CompactViewState() = default;
    CompactViewState(State* state, size_type offset, size_type count)
        : state_{state}
        , offset_{offset}
        , count_{count} {
        validate();
    }
    CompactViewState(CompactViewState const&) = default;
    auto operator=(CompactViewState const&) -> CompactViewState& = default;
    CompactViewState(CompactViewState<false, Require> const& other)
        requires Const
        : state_{other.state_}
        , offset_{other.offset_}
        , count_{other.count_} {}

    void validate() const {
        Require(offset_ >= 0 && count_ >= 0);
        Require(state_ ? offset_ <= state_->num_ && count_ <= state_->num_ - offset_
                       : offset_ == 0 && count_ == 0);
    }
    auto num() const noexcept -> size_type { return count_; }
    auto is_empty() const noexcept -> bool { return count_ == 0; }
    auto get_view(this auto const& self) { return self; }
    auto get_view(this auto const& self, size_type offset, size_type count) {
        return self.slice(offset, count);
    }
    auto slice(this auto const& self, size_type offset, size_type count) {
        self.validate();
        Require(offset >= 0 && offset <= self.count_ && count >= 0 &&
                count <= self.count_ - offset);
        using Self = std::remove_cvref_t<decltype(self)>;
        return Self{self.state_, self.offset_ + offset, count};
    }
    auto left(this auto const& self, size_type count) { return self.slice(0, count); }
    auto right(this auto const& self, size_type count) {
        Require(count >= 0 && count <= self.count_);
        return self.slice(self.count_ - count, count);
    }
  protected:
    template <bool, auto>
    friend struct CompactViewState;
    auto capacity_blocks() const -> std::size_t {
        return state_ ? static_cast<std::size_t>(state_->capacity_ /
                                                 single_allocation_layout::capacity_granularity)
                      : 0;
    }
    template <typename T>
    auto column_data(std::size_t byte_offset) const -> Element<T>* {
        validate();
        if (!state_ || !state_->data_) {
            return nullptr;
        }
        return column_data_unchecked<T>(byte_offset);
    }
    template <typename T>
    auto column_data_unchecked(std::size_t byte_offset) const -> Element<T>* {
        return std::launder(reinterpret_cast<Element<T>*>(state_->data_ + byte_offset)) + offset_;
    }
    State* state_{};
    size_type offset_{};
    size_type count_{};
};

}
