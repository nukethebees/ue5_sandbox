#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace ml::soa_storage_detail {

template <typename Column, int Dimensions>
struct VectorColumns;

template <typename Column>
struct VectorColumns<Column, 2> {
    Column xs;
    Column ys;
};

template <typename Column>
struct VectorColumns<Column, 3> {
    Column xs;
    Column ys;
    Column zs;
};

template <typename T, int Dimensions, template <typename> typename Span, auto Require>
class VectorView {
    static_assert(Dimensions == 2 || Dimensions == 3);
    static_assert(std::is_arithmetic_v<T> && !std::is_volatile_v<T>);
    using Byte = std::conditional_t<std::is_const_v<T>, std::byte const, std::byte>;
  public:
    using size_type = std::int32_t;
    using View = VectorView<std::remove_const_t<T>, Dimensions, Span, Require>;
    using ConstView = VectorView<std::add_const_t<T>, Dimensions, Span, Require>;

    /* **************************************** */
    // Lifetime
    /* **************************************** */
    VectorView() = default;
    VectorView(T* first, std::size_t byte_stride, size_type count)
        : data_{reinterpret_cast<Byte*>(first)}
        , count_{count} {
        Require(count >= 0 && (first != nullptr || count == 0));
        Require(byte_stride % alignof(T) == 0 &&
                byte_stride <= std::numeric_limits<std::uint32_t>::max());
        Require(byte_stride >= static_cast<std::size_t>(count) * sizeof(T));
        byte_stride_ = static_cast<std::uint32_t>(byte_stride);
    }
    template <typename U>
        requires (std::is_const_v<T> && std::is_same_v<U, std::remove_const_t<T>>)
    VectorView(VectorView<U, Dimensions, Span, Require> const& other)
        : data_{other.data_}
        , byte_stride_{other.byte_stride_}
        , count_{other.count_} {}

    /* **************************************** */
    // Columns
    /* **************************************** */
    auto num() const noexcept -> size_type { return count_; }
    auto is_empty() const noexcept -> bool { return count_ == 0; }
    auto byte_stride() const noexcept -> std::size_t { return byte_stride_; }
    auto xs() const -> Span<T> { return column(0); }
    auto ys() const -> Span<T> { return column(1); }
    auto zs() const -> Span<T>
        requires (Dimensions == 3)
    {
        return column(2);
    }
    auto columns() const -> VectorColumns<Span<T>, Dimensions> {
        if constexpr (Dimensions == 2) {
            return {xs(), ys()};
        } else {
            return {xs(), ys(), zs()};
        }
    }
    template <typename Func>
    auto apply_arrays(Func&& func) const -> decltype(auto) {
        auto values{columns()};
        if constexpr (Dimensions == 2) {
            return std::forward<Func>(func)(values.xs, values.ys);
        } else {
            return std::forward<Func>(func)(values.xs, values.ys, values.zs);
        }
    }
    template <typename Func>
    void each_column(Func&& func) const {
        func(xs());
        func(ys());
        if constexpr (Dimensions == 3) {
            func(zs());
        }
    }

    /* **************************************** */
    // Subviews
    /* **************************************** */
    auto get_view() const -> VectorView { return *this; }
    auto get_const_view() const -> ConstView { return *this; }
    auto slice(size_type offset, size_type count) const -> VectorView {
        Require(offset >= 0 && offset <= count_ && count >= 0 && count <= count_ - offset);
        auto result{*this};
        if (result.data_) {
            result.data_ += static_cast<std::size_t>(offset) * sizeof(T);
        }
        result.count_ = count;
        return result;
    }
    auto get_view(size_type offset, size_type count) const -> VectorView {
        return slice(offset, count);
    }
    auto get_const_view(size_type offset, size_type count) const -> ConstView {
        return slice(offset, count);
    }
    auto left(size_type count) const -> VectorView { return slice(0, count); }
    auto right(size_type count) const -> VectorView {
        Require(count >= 0 && count <= count_);
        return slice(count_ - count, count);
    }
  private:
    template <typename, int, template <typename> typename, auto>
    friend class VectorView;
    auto column(std::size_t index) const -> Span<T> {
        auto* pointer{data_ ? reinterpret_cast<T*>(data_ + index * byte_stride()) : nullptr};
        if constexpr (requires { typename Span<T>::size_type; }) {
            return Span<T>{pointer, static_cast<typename Span<T>::size_type>(count_)};
        } else {
            return Span<T>{pointer, count_};
        }
    }
    Byte* data_{};
    std::uint32_t byte_stride_{};
    size_type count_{};
};

} // namespace ml::soa_storage_detail
