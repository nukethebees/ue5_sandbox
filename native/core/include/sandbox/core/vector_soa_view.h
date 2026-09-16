#pragma once

#include "sandbox/core/math_types.h"

#include <cassert>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ml {
template <typename T>
    requires std::is_same_v<std::remove_const_t<T>, float>
struct Vector3SoAView {
    using value_type = T;
    using size_type = std::int32_t;
    using View = Vector3SoAView<float>;
    using ConstView = Vector3SoAView<float const>;
    using equivalent_type = Vector3f;

    Vector3SoAView() = default;
    Vector3SoAView(T* const in_xs,
                   T* const in_ys,
                   T* const in_zs,
                   size_type const in_count) noexcept
        : xs{in_xs}
        , ys{in_ys}
        , zs{in_zs}
        , count{in_count} {
        assert(count >= 0);
        assert(count == 0 || (xs != nullptr && ys != nullptr && zs != nullptr));
    }
    Vector3SoAView(std::span<T> const in_xs,
                   std::span<T> const in_ys,
                   std::span<T> const in_zs) noexcept
        : Vector3SoAView{
              in_xs.data(), in_ys.data(), in_zs.data(), static_cast<size_type>(in_xs.size())} {
        assert(in_xs.size() == in_ys.size());
        assert(in_xs.size() == in_zs.size());
    }
    template <typename U>
        requires (std::is_const_v<T> && std::is_same_v<U, std::remove_const_t<T>>)
    Vector3SoAView(Vector3SoAView<U> const other) noexcept
        : xs{other.xs}
        , ys{other.ys}
        , zs{other.zs}
        , count{other.count} {}

    auto operator[](size_type const index) const noexcept -> equivalent_type {
        assert(index >= 0 && index < count);
        return make_vector3f(xs[index], ys[index], zs[index]);
    }
    void set(size_type const index, equivalent_type const value) const noexcept
        requires (!std::is_const_v<T>)
    {
        set(index, value.X, value.Y, value.Z);
    }
    void set(size_type const index, float const x, float const y, float const z) const noexcept
        requires (!std::is_const_v<T>)
    {
        assert(index >= 0 && index < count);
        xs[index] = x;
        ys[index] = y;
        zs[index] = z;
    }

    auto num() const noexcept -> size_type { return count; }
    auto is_empty() const noexcept -> bool { return count == 0; }
    auto xs_span() const noexcept -> std::span<T> { return {xs, static_cast<std::size_t>(count)}; }
    auto ys_span() const noexcept -> std::span<T> { return {ys, static_cast<std::size_t>(count)}; }
    auto zs_span() const noexcept -> std::span<T> { return {zs, static_cast<std::size_t>(count)}; }
    template <typename Fn>
    void each_column(Fn&& fn) const {
        auto xs_view{xs_span()};
        auto ys_view{ys_span()};
        auto zs_view{zs_span()};
        fn(xs_view);
        fn(ys_view);
        fn(zs_view);
    }
    void validate_array_sizes() const noexcept {
        assert(count >= 0);
        assert(count == 0 || (xs != nullptr && ys != nullptr && zs != nullptr));
    }

    auto get_view() const noexcept -> Vector3SoAView { return *this; }
    auto get_const_view() const noexcept -> ConstView { return *this; }
    auto slice(size_type const offset, size_type const slice_count) const noexcept
        -> Vector3SoAView {
        assert(offset >= 0 && slice_count >= 0 && offset <= count && slice_count <= count - offset);
        return {xs == nullptr ? nullptr : xs + offset,
                ys == nullptr ? nullptr : ys + offset,
                zs == nullptr ? nullptr : zs + offset,
                slice_count};
    }
    auto get_view(size_type const offset, size_type const slice_count) const noexcept
        -> Vector3SoAView {
        return slice(offset, slice_count);
    }
    auto get_const_view(size_type const offset, size_type const slice_count) const noexcept
        -> ConstView {
        return slice(offset, slice_count);
    }
    auto left(size_type const slice_count) const noexcept -> Vector3SoAView {
        return slice(0, slice_count);
    }
    auto right(size_type const slice_count) const noexcept -> Vector3SoAView {
        return slice(count - slice_count, slice_count);
    }

    T* xs{};
    T* ys{};
    T* zs{};
    size_type count{};
};

using Vector3fSoAView = Vector3SoAView<float>;
using Vector3fSoAConstView = Vector3SoAView<float const>;

static_assert(sizeof(Vector3fSoAView) == 32);
static_assert(sizeof(Vector3fSoAConstView) == 32);
static_assert(std::is_trivially_copyable_v<Vector3fSoAView>);
static_assert(std::is_trivially_copyable_v<Vector3fSoAConstView>);
}
