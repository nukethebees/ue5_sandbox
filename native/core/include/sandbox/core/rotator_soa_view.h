#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace ml::soa_storage_detail {

template <typename T,
          template <typename> typename Span,
          typename Size = std::uint32_t,
          typename Value = std::array<std::remove_const_t<T>, 3>>
class RotatorSoAView {
  public:
    using size_type = Size;
    using View = RotatorSoAView<std::remove_const_t<T>, Span, Size, Value>;
    using ConstView = RotatorSoAView<std::add_const_t<T>, Span, Size, Value>;
    using value_type = Value;
    using equivalent_type = Value;

    /* **************************************** */
    // Lifetime
    /* **************************************** */
    RotatorSoAView() = default;
    RotatorSoAView(T* pitches, T* yaws, T* rolls, size_type count)
        : pitches_{pitches}
        , yaws_{yaws}
        , rolls_{rolls}
        , count_{count} {
        assert(count >= 0 &&
               (count == 0 || (pitches != nullptr && yaws != nullptr && rolls != nullptr)));
    }
    template <typename U>
        requires (std::is_const_v<T> && std::is_same_v<U, std::remove_const_t<T>>)
    RotatorSoAView(RotatorSoAView<U, Span, Size, Value> const& other)
        : pitches_{other.pitches_}
        , yaws_{other.yaws_}
        , rolls_{other.rolls_}
        , count_{other.count_} {}

    /* **************************************** */
    // Columns
    /* **************************************** */
    auto num() const noexcept -> size_type { return count_; }
    auto is_empty() const noexcept -> bool { return count_ == 0; }
    auto pitches() const -> Span<T> { return {pitches_, count_}; }
    auto yaws() const -> Span<T> { return {yaws_, count_}; }
    auto rolls() const -> Span<T> { return {rolls_, count_}; }
    auto operator[](size_type const index) const -> value_type {
        assert(index >= 0 && index < count_);
        return {pitches_[index], yaws_[index], rolls_[index]};
    }
    template <typename Func>
    auto apply_arrays(Func&& func) const -> decltype(auto) {
        auto pitch_values{pitches()};
        auto yaw_values{yaws()};
        auto roll_values{rolls()};
        return std::forward<Func>(func)(pitch_values, yaw_values, roll_values);
    }
    template <typename Func>
    void each_column(Func&& func) const {
        func(pitches());
        func(yaws());
        func(rolls());
    }

    /* **************************************** */
    // Subviews
    /* **************************************** */
    auto get_view() const -> RotatorSoAView { return *this; }
    auto get_const_view() const -> ConstView { return *this; }
    auto slice(size_type offset, size_type count) const -> RotatorSoAView {
        assert(offset >= 0 && offset <= count_ && count >= 0 && count <= count_ - offset);
        return {pitches_ == nullptr ? nullptr : pitches_ + offset,
                yaws_ == nullptr ? nullptr : yaws_ + offset,
                rolls_ == nullptr ? nullptr : rolls_ + offset,
                count};
    }
    auto get_view(size_type offset, size_type count) const -> RotatorSoAView {
        return slice(offset, count);
    }
    auto get_const_view(size_type offset, size_type count) const -> ConstView {
        return slice(offset, count);
    }
    auto left(size_type count) const -> RotatorSoAView { return slice(0, count); }
    auto right(size_type count) const -> RotatorSoAView {
        assert(count >= 0 && count <= count_);
        return slice(count_ - count, count);
    }
  private:
    template <typename, template <typename> typename, typename, typename>
    friend class RotatorSoAView;
    T* pitches_{};
    T* yaws_{};
    T* rolls_{};
    size_type count_{};
};

} // namespace ml::soa_storage_detail
